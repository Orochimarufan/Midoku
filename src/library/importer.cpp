#include <memory>
#include <unordered_map>

#include <QMimeDatabase>
#include <QDebug>
#include <QImage>
#include <QImageReader>
#include <QBuffer>
#include <QCollator>

#include "importer.h"
#include "importer_metadata.h"
#include "book.h"
#include "chapter.h"
#include "blob.h"

namespace fs = std::filesystem;


#if !QT_VERSION_CHECK(5, 14, 0)
namespace std {
template <>
struct hash<QString> {
    size_t operator()(const QString &s) const {
        return qHash(s);
    }
};
}
#endif

namespace Midoku::Library {

const QSet<QString> Importer::supported_types {
    //"audio/ogg",
    "audio/x-opus+ogg",
    "audio/x-vorbis+ogg",
    "audio/x-flac+ogg",
    //"audio/flac",
    "audio/mpeg",
    "audio/x-m4b",
    "audio/x-m4a",
    "audio/mp4"
};

Importer::Importer(Database &db) :
    db(db)
{
}

DBResult<std::unordered_set<std::string>> Importer::inventory()
{
    auto q = db.prepare("SELECT media FROM Chapter;");
    if (q.exec()) {
        std::unordered_set<std::string> inv;
        while (q.next())
            inv.emplace(q.value(0).toString().toStdString());
        return Ok(inv);
    }
    return Err(q.lastError());
}


// -----------------------------------------------------------------------------
// Insertion
static inline std::string str_lower(const std::string &s) {
    std::string out(s.size(), 0);
    std::transform(s.begin(), s.end(), out.begin(), [](unsigned char c) {return std::tolower(c);});
    return out;
}

static inline std::set<QString> qstr_words(const QString &s) {
    std::set<QString> res;
    QStringList sl = s.split(' ');
    std::copy(sl.begin(), sl.end(), std::inserter(res, res.begin()));
    return res;
}


template <typename Coll, typename F, typename... Args>
static inline typename Coll::iterator find_or_emplace(Coll &c, const typename Coll::key_type &k, F value_fn) {
    auto it = c.find(k);
    if (it == c.end())
        std::tie(it, std::ignore) = c.emplace(k, value_fn());
    return it;
}

template <typename F, typename Coll>
static inline std::unordered_map<std::invoke_result_t<F, typename Coll::value_type>, std::vector<typename Coll::value_type>> group_by(const Coll &src, F key_func) {
    using value_type = typename Coll::value_type;
    std::unordered_map<std::invoke_result_t<F, value_type>, std::vector<value_type>> res;
    typename decltype(res)::iterator it;

    for (const value_type &x : src)
        find_or_emplace(res, key_func(x), [] { return std::vector<value_type>(); })->second.push_back(x);

    return res;
}


DBResult<void> Importer::process_dir(const std::set<fs::path> &files) {
    QMimeDatabase mimedb;

    // Analyze files
    std::vector<MediaInfo> items;
    QImage cover_file;

    qDebug() << "Processing directory:" << (*files.cbegin()).parent_path().filename().c_str();

    for (auto &file : files) {
        auto qname = QString::fromStdString(file);
        auto type = mimedb.mimeTypeForFile(qname);

        if      (type.inherits("audio/ogg"))
            items.emplace_back(ogg_process(file, type));
        else if (type.inherits("audio/x-m4a")
              || type.inherits("audio/mp4")
              || type.inherits("audio/x-m4b"))
            items.emplace_back(isobmff_process(file, type));
        else if (type.inherits("audio/mpeg"))
            items.emplace_back(mpeg_process(file));
        else if (QImageReader::supportedMimeTypes().contains(type.name().toUtf8())) {
            auto ciname = str_lower(file.filename().native());
            if (cover_file.isNull())
                //if (!ciname.find("cover"))
                    cover_file = QImage(qname);
        }
    }

    // Try to normalize title
    {
        std::unordered_map<QString, std::pair<int, QString>> title_count;

        for (auto &item : items)
            find_or_emplace(title_count, item.title.toLower(), [&item] { return std::pair{0, item.title}; })->second.first += 1;

        struct temp {
            std::set<QString> words;
            int weight;
            QString original;

            bool operator <(const temp &o) const {
                return weight < o.weight;
            }

            bool operator ==(const temp &o) const {
                return weight == o.weight && words == o.words;
            }
        };

        std::set<temp> tcs;

        std::transform(title_count.begin(), title_count.end(), std::inserter(tcs, tcs.begin()), [] (auto ppis) {
            return temp{qstr_words(ppis.first), ppis.second.first, ppis.second.second};
        });

        for (auto &item : items) {
            std::set<QString> words = qstr_words(item.title.toLower());
            for (auto &t : tcs) {
                // TODO: Be smarter...
                std::vector<QString> match;
                match.reserve(words.size());
                std::set_union(words.begin(), words.end(), t.words.begin(), t.words.end(), std::back_inserter(match));
                if (match.size() == std::min(words.size(), t.words.size()))
                    item.title = t.original;
            }
        }
    }

    // Sort into books by title
    auto books = group_by(items, [] (const MediaInfo &i) { return i.title; });

    // Update DB
    for (auto &it : books) {
        // TODO: Also consider author and reader above?
        const MediaInfo &info = it.second.front();
        auto r = db.select<Book>(Book::title == info.title)
                    .bind([this, &info, &it, cover_file] (std::vector<std::unique_ptr<Book>> &&books) -> DBResult<void> {
                std::unique_ptr<Book> b;
                if (books.size() < 1) {
                    // Create new book
                    b = std::make_unique<Book>(db, info.title, info.author, info.reader);
                    auto r = b->save();
                    if (!r) return r.discard();
                } else if (books.size() > 1) {
                    // TODO: Deal with it?
                    assert(false && "More than one book with the same title");
                } else {
                    // Found book in database
                    b = std::move(books[0]);
                }

                qDebug() << "Found Book" << info.title << "by" << info.author;

                // Handle cover
                if (!cover_file.isNull() && !b->get(Book::cover_blob_id).has_value()) {
                    // Scale image
                    QByteArray data;
                    QBuffer buf(&data);
                    buf.open(QIODevice::WriteOnly);
                    cover_file.scaled(500, 500, Qt::KeepAspectRatio).save(&buf, "PNG");
                    buf.close();

                    // Save blob
                    Blob cover(db, data);
                    auto r = cover.save().bind([&b] (long id) {
                        b->set(Book::cover_blob_id, id);
                        return b->save();
                    });
                    if (!r) return r.discard();
                }

                auto mediae = it.second;

                // Detect broken chapters
                int renumber = [&](){
                    if (mediae.size() <= 1 && !mediae[0].multi_chapter)
                        return 0;
                    std::vector<int> numbers;
                    for (auto &info : mediae)
                        for (auto &chap : info.chapters)
                            numbers.push_back(chap.no);
                    std::sort(numbers.begin(), numbers.end());
                    int expect = 1;
                    int result = 0;
                    for (int i : numbers) {
                        if (i & 0xFFFF0000)
                            result = 2;
                        if (i < expect)
                            return 1;
                        else
                            expect = i;
                    }
                    return result;
                }();


                if (renumber == 1) {
                    // Last resort: renumber by filename sorting
                    // Make sure filenames are sorted if we're renumbering
                    QCollator collate;
                    collate.setNumericMode(true);
                    std::sort(mediae.begin(), mediae.end(), [&collate](auto& a, auto& b) {
                        return collate.compare(a.media, b.media) < 0;
                    });
                } else if (renumber == 2) {
                    // renumber by old chapter numbers, but still renumber
                    std::sort(mediae.begin(), mediae.end(), [](auto& a, auto& b) {
                        return a.chapters.front().no < b.chapters.front().no;
                    });
                }

                // Add chapters
                int renumber_count = 0;
                for (auto &info : mediae) {
                    // Add chapters from info
                    int mc_count = 0;
                    for (auto & chap : info.chapters) {
                        long chapter_no = renumber? ++renumber_count: chap.no;
                        // only set media_chapter for files that actually contain multiple chapters
                        std::optional<int> mc = info.multi_chapter ? std::optional(++mc_count) : std::nullopt;
                        qDebug() << "> Found Chapter" << chapter_no << chap.name << "from" << info.media.mid(info.media.lastIndexOf('/')+1) << mc.value_or(0);
                        Chapter c(db, *b, chapter_no, chap.get_length(), info.media, chap.start, mc, chap.name);
                        auto r = c.save();
                        if (!r)
                            return r.discard();
                    }
                }

                return Ok();
            });

        if (!r) return r;
    }

    return Ok();
}

DBResult<void> Importer::import(const std::vector<fs::path> &paths) {
    return inventory().bind([this, &paths] (std::unordered_set<std::string> &&inventory) -> DBResult<void> {
        std::unordered_map<std::string, std::set<fs::path>> found;

        for (auto& search_path : paths)
            for (auto &ent : fs::recursive_directory_iterator(fs::absolute(search_path),
                                                              fs::directory_options::follow_directory_symlink|fs::directory_options::skip_permission_denied))
                if ((ent.is_regular_file() || ent.is_symlink())) {
                    auto path = ent.path();
                    // Check if file is already in DB
                    auto inv_el_it = inventory.find(path);
                    if (inv_el_it == inventory.end()) {
                        // Found new file
                        find_or_emplace(found, path.parent_path(), [] { return std::set<fs::path>(); })->second.emplace(path);
                    } else {
                        // Remove from inventory to confirm it still exists
                        inventory.erase(inv_el_it);
                    }
                }

        // TODO: files remaining in inventory have been (re)moved

        const int count = found.size();
        int prog = 0;
        for (auto &dir : found) {
            // TODO: report progress per file?
            emit progress(prog++, count, QString::fromStdString(dir.first));

            auto r = process_dir(dir.second);
            //if (!r) return r; // FIXME
        }

        return Ok();
    });
}

}
