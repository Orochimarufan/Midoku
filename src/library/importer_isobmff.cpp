#include "importer_metadata.h"

#include <taglib/mp4file.h>
#include <taglib/tpropertymap.h>

#include <iostream>
#include <fstream>
#include <endian.h>
#include <array>
#include <utility>
#include <cstring>

#include <QDebug>


namespace Midoku::Library {

// ----------------------------------------------------------------------------------------------------------
// Atom parsing
namespace {

struct atom_header {
    size_t size = 0;
    char type[4] = {0,0,0,0};
    size_t offset = 0;
    size_t header_size = 0;

    friend std::istream & operator>>(std::istream &input, atom_header &self) {
        self.offset = input.tellg();
        uint32_t size_be;
        input.read((char*)&size_be, 4);
        input.read(self.type, 4);
        if (size_be == htobe32(1)) {
            // 64-bit size
            uint64_t size64_be;
            input.read((char*)&size64_be, 8);
            self.size = be64toh(size64_be);
        } else if (size_be == htobe32(0)) {
            std::cout << "size 0" << std::endl;
            // rest of file
            size_t here = input.tellg();
            input.seekg(0, std::ios_base::end);
            self.size = ((size_t)input.tellg()) - here;
            input.seekg(0, std::ios_base::beg);
        } else {
            self.size = be32toh(size_be);
        }
        self.header_size = ((size_t)input.tellg()) - self.offset;
        return input;
    }

    static constexpr std::array<const char[5],10> _CONTAINERS {{
        "moov", "udta", "trak", "mdia", "meta", "ilst", "stbl", "mind", "moof", "traf"
    }};
    static constexpr std::array<std::pair<const char*, unsigned>, 1> _SKIP_SIZE {{
        {"meta", 4}
    }};

    inline operator bool() const {
        return header_size != 0;
    }

    inline bool is_container() const {
        for (const char *ct : _CONTAINERS) {
            if (!memcmp(type, ct, 4))
                return true;
        }
        return false;
    }

    inline bool is(const char *tp) const {
        return !strncmp(type, tp, 4);
    }

    inline atom_header &next(std::istream &input) {
        input.seekg(offset + size, std::ios_base::beg);
        input >> *this;
        return *this;
    }

    inline size_t data_offset() const {
        return offset + header_size;
    }

    inline size_t data_size() const {
        return size - (header_size);
    }

    inline std::vector<uint8_t> read_data(std::istream &input) const {
        input.seekg(data_offset());
        auto res = std::vector<uint8_t>(data_size());
        input.read((char*)res.data(), data_size());
        return res;
    }

    inline size_t children_offset() const {
        for (auto [t, n] : _SKIP_SIZE) {
            if (!memcmp(type, t, 4))
                return offset + header_size + n;
        }
        return offset + header_size;
    }

    inline size_t children_size() const {
        for (auto [t, n] : _SKIP_SIZE) {
            if (!memcmp(type, t, 4)) {
                return size - header_size - n;
            }
        }
        return size - header_size;
    }

    inline size_t end_offset() const {
        return offset + size;
    }
};

class AtomParser {
    std::istream &_istream;
    atom_header _cur;
    atom_header _parent;

public:
    AtomParser(std::istream &stream) :
        _istream(stream)
    {
        _istream.seekg(0, std::ios_base::end);
        _parent.size = _istream.tellg();
        if (_parent.size < 8) {
            _cur = {};
        } else {
            _istream.seekg(0, std::ios_base::beg);
            _istream >> _cur;
        }
    }

    AtomParser(std::istream &stream, const atom_header &parent) :
        _istream(stream),
        _parent(parent)
    {
        _istream.seekg(_parent.data_offset());
        if (_parent.children_size() > 8)
            _istream >> _cur;
        else
            _cur = {};
    }

    AtomParser(const AtomParser &) = delete;

    struct end_iterator {};

    bool operator !=(const AtomParser &o) const {
        return !*this == !o && _cur.offset != o._cur.offset;
    }

    bool operator !=(const end_iterator &) const {
        return _cur;
    }

    operator bool() const {
        return _cur;
    }

    AtomParser &begin() {
        return *this;
    }

    end_iterator end() const {
        return end_iterator();
    }

    AtomParser &operator ++() {
        if (_cur.end_offset() + 8 <= _parent.end_offset())
            _cur.next(_istream);
        else
            _cur = {};
        return *this;
    }

    AtomParser &operator *() {
        return *this;
    }

    const atom_header &parent() const {
        return _parent;
    }

    const atom_header &current() const {
        return _cur;
    }

    const atom_header *operator ->() const {
        return &_cur;
    }

    std::vector<uint8_t> read_data() {
        return _cur.read_data(_istream);
    }

    AtomParser children() {
        return AtomParser(_istream, _cur);
    }
};

struct atom_mvhd {
    uint8_t version;
    int32_t timescale;
    uint64_t duration;

    friend std::istream & operator >>(std::istream &stream, atom_mvhd &self) {
        stream.read((char*)&self.version, 1);
        if (self.version == 0) {
            stream.seekg(11, std::ios_base::cur);
            int32_t timescale_be;
            stream.read((char*)&timescale_be, 4);
            uint32_t duration_be;
            stream.read((char*)&duration_be, 4);
            self.timescale = be32toh(timescale_be);
            self.duration = be32toh(duration_be);
        } else if (self.version == 1) {
            stream.seekg(19, std::ios_base::cur);
            int32_t timescale_be;
            stream.read((char*)&timescale_be, 4);
            uint64_t duration_be;
            stream.read((char*)&duration_be, 8);
            self.timescale = be32toh(timescale_be);
            self.duration = be64toh(duration_be);
        }
        return stream;
    }
};

struct atom_chpl_entry {
    uint64_t start;
    std::string title;

    friend std::istream &operator >>(std::istream &stream, atom_chpl_entry &self) {
        uint64_t start_be;
        stream.read((char*)&start_be, 8);
        self.start = be64toh(start_be);
        uint8_t title_len;
        stream.read((char*)&title_len, 1);
        self.title.resize(title_len);
        stream.read(self.title.data(), title_len);
        return stream;
    }
};

struct atom_chpl_header {
    uint8_t chapters;

    friend std::istream &operator >>(std::istream &stream, atom_chpl_header &self) {
        stream.seekg(8, std::ios_base::cur);
        stream.read((char*)&self.chapters, 1);
        return stream;
    }
};

template <typename T>
T atom(std::istream &stream, const atom_header &h) {
    T data;
    stream.seekg(h.data_offset(), std::ios_base::beg);
    stream >> data;
    return data;
}

}

// ----------------------------------------------------------------------------------------------------------
// Metadata
static bool isobmff_chapters(ImportState &import, const std::filesystem::path &p) {
    auto fs = std::ifstream(p, std::ios_base::binary);

    atom_header mvhd_atom;
    atom_header chpl_atom;

    for (auto a = AtomParser(fs); a; ++a) {
        if (a->is("moov")) {
            for (auto b = a.children(); b; ++b) {
                if (b->is("mvhd")) {
                    mvhd_atom = b.current();
                } else if (b->is("udta")) {
                    for (auto a = b.children(); a; ++a) {
                        if (a->is("chpl")) {
                            chpl_atom = a.current();
                            break;
                        }
                    }
                }
            }
        }
    }

    if (!mvhd_atom || !chpl_atom) {
        std::cerr << "ISOBMFF/M4B: No mvhd and/or chpl found" << std::endl;
        return false;
    }

    // Parse mvhd
    auto mvhd = atom<atom_mvhd>(fs, mvhd_atom);
    auto scale = mvhd.timescale * 10000;

    // Parse chpl
    auto chpl = atom<atom_chpl_header>(fs, chpl_atom);
    auto entry = atom_chpl_entry{};
    for (int i = 1; i <= chpl.chapters; i++) {
        fs >> entry;
        import.chaps.emplace(i, ChapterInfo{.no=i, .start=(int64_t)(entry.start/scale), .name=QString::fromStdString(entry.title)});
        qDebug() << entry.start / scale << ":" << entry.title.c_str();
    }

    return !import.chaps.empty();
}

MediaInfo isobmff_process(const std::filesystem::path &file, const QMimeType &type) {
    Q_UNUSED(type)
    ImportState s(file);

    // TagLib
    {
        TagLib::MP4::File isobmf(file.c_str());
        s.readAudioProperties(isobmf.audioProperties());

        // Read simple stuff
        for (auto &kv : isobmf.properties()) {
            auto tag = kv.first.upper();

            if (s.handleCommonTags(tag, kv.second))
                ;
            else if (tag == "COMPOSER")
                s.info.author = tqstr(kv.second);
            else
                qDebug() << "Unknown MP4 Tag:" << tag.toCString(true);
        }

        // Now for the complicated stuff
        // Cover art
        s.info.cover = [&isobmf]() {
            auto items = isobmf.tag()->itemMap();
            auto covr_it = items.find("covr");
            if (covr_it != items.end()) {
                auto covr_list = covr_it->second.toCoverArtList();
                if (!covr_list.isEmpty()) {
                    auto covr = covr_list.front();
                    auto data = covr.data();
                    return QImage::fromData(reinterpret_cast<const uint8_t*>(data.data()),
                                                    data.size(),
                                                    [&covr]() -> const char* {
                                                        switch (covr.format()) {
                                                        case TagLib::MP4::CoverArt::GIF:
                                                            return "GIF";
                                                        case TagLib::MP4::CoverArt::BMP:
                                                            return "BMP";
                                                        case TagLib::MP4::CoverArt::PNG:
                                                            return "PNG";
                                                        case TagLib::MP4::CoverArt::JPEG:
                                                            return "JPEG";
                                                        case TagLib::MP4::CoverArt::Unknown:
                                                        default:
                                                            return nullptr;
                                                        }
                                                    }());
                }
            }
            return QImage();
        }();
    }

    // Chapters
    // Chapters in ISOBMFF are a royal pain.
    isobmff_chapters(s, file);

    s.finalize();

    return s.info;
}

}
