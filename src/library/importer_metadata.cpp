#include <taglib/opusfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/oggflacfile.h>
#include <taglib/xiphcomment.h>
#include <taglib/mpegfile.h>
#include <taglib/tpropertymap.h>

#include <QDebug>

#include "importer_metadata.h"

namespace fs = std::filesystem;


// ----------------------------------------------------------------------------
// Metadata parsing

namespace Midoku::Library {

bool ImportState::finalize() {
    // Do Author/Reader guessing
    if (!artist.isNull()) {
        if (info.author.isNull())
            info.author = artist;
        else if (info.reader.isNull())
            info.reader = artist;
    }

    // Handle chapters
    assert(length != 0);
    if (!chaps.size()) {
        // File is single chapter
        info.multi_chapter = false;

        int no = 0; // FIXME: deal with broken metadata in a way that doesn't trigger the unique constraint. Might want to sort files into containing folders and try to guess from file order
        if (!track.isNull()) {
            if (track.contains('/'))
                no = track.split('/')[0].toInt();
            else
                no = track.toInt();
        }

        // Add a single chapter
        if (title.isNull())
            title = guess_title_from_filename(path);

        info.chapters.emplace_back(ChapterInfo{no, 0, length, length, title});

        // Check book title
        if (album.isNull())
            info.title = guess_title_from_directory(path);
        else
            info.title = album;
    } else {
        // File contains multiple chapters
        info.multi_chapter = true;
        info.chapters.resize(chaps.size());
        for (auto &chap : chaps)
            info.chapters[chap.first-1] = chap.second;

        for (size_t i = 0; i<chaps.size() - 1; i++)
            if (info.chapters[i].end < 0 && info.chapters[i].length < 0)
                info.chapters[i].end = info.chapters[i+1].start;
        info.chapters[-1].end = length;

        // In this case, prefer to use title tag for book title
        if (title.isNull()) {
            if (album.isNull())
                info.title = guess_title_from_filename(path); // same as above
            else
                info.title = album;
        } else
            info.title = title;
    }

    return true;
}


// Ogg
static inline int ogg_parse_chapter_start(const TagLib::String &str) {
    int h, m, s, frac;
    std::wistringstream stream (str.toWString());
    stream >> h;
    stream.ignore(1, ':');
    stream >> m;
    stream.ignore(1, ':');
    stream >> s;
    stream.ignore(1, '.');
    stream >> frac;

    return h*3600 + m*60 + s;
}

MediaInfo ogg_process(const fs::path &file, const QMimeType &type) {
    ImportState s(file);

    auto ogg = [&file, &type] () -> std::unique_ptr<TagLib::Ogg::File> {
        if (type.inherits("audio/x-opus+ogg"))
            return std::make_unique<TagLib::Ogg::Opus::File>(file.c_str());
        else if (type.inherits("audio/x-vorbis+ogg"))
            return std::make_unique<TagLib::Ogg::Vorbis::File>(file.c_str());
        else if (type.inherits("audio/x-flac+ogg"))
            return std::make_unique<TagLib::Ogg::FLAC::File>(file.c_str());
        else
            assert(false && "Unknown OGG file type");
    }();

    s.readAudioProperties(ogg->audioProperties());

    for (auto &kv : ogg->properties()) {
        auto tag = kv.first.upper();

        if (s.handleCommonTags(tag, kv.second))
            ;
        else if (!tag.find("CHAPTER")) {
            int num = tag.substr(7, 3).toInt();

            auto it = s.chaps.find(num);
            if (it == s.chaps.end()) {
                auto res = s.chaps.emplace(num, ChapterInfo{num});
                it = res.first;
            }

            if (tag.size() == 10)
                it->second.start = ogg_parse_chapter_start(kv.second.back());
            if (tag.rfind("NAME") != -1)
                it->second.name = tqstr(kv.second);
        }
        else
            qDebug() << "Unknown OGG TAG:" << tag.toCString() << "=" << tqstr(kv.second);
    }

    // Pictures
    {
        auto comment = static_cast<TagLib::Ogg::XiphComment*>(ogg->tag());

        static constexpr size_t n = 3;
        TagLib::FLAC::Picture * imgs[n];
        std::fill(imgs, imgs+n, nullptr);

        for (auto pic : comment->pictureList()) {
            if      (pic->type() == TagLib::FLAC::Picture::Media)
                imgs[0] = pic;
            else if (pic->type() == TagLib::FLAC::Picture::FrontCover)
                imgs[1] = pic;
            else if (pic->type() == TagLib::FLAC::Picture::Other)
                imgs[2] = pic;
        }

        for (size_t i = 0; i < n; i++)
            if (imgs[i]) {
                auto data = imgs[i]->data();
                s.info.cover = QImage::fromData((uint8_t *)data.data(), data.size());
                break;
            }
    }

    s.finalize();

    return s.info;
}

// MP3
MediaInfo mpeg_process(const fs::path &file) {
    ImportState s(file);
    TagLib::MPEG::File mp3(file.c_str());

    s.readAudioProperties(mp3.audioProperties());

    for (auto &kv : mp3.properties()) {
        auto tag = kv.first.upper();

        if (s.handleCommonTags(tag, kv.second))
            ;
        else
            qDebug() << "Unknown MP3 TAG:" << tag.toCString() << "=" << tqstr(kv.second);
    }

    s.finalize();

    return s.info;
}

}
