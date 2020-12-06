#pragma once

#include <array>
#include <fstream>
#include <memory>
#include <cstring>
#include <vector>


namespace isobmff {

// ----------------------------------------------------------------------------------------------------------
// Atom parsing

template <typename T>
T readbe(std::istream &stream) {
    T be;
    stream.read((char*)&be, sizeof(T));
    if constexpr (sizeof(T) == 8)
        return be64toh(be);
    else if constexpr (sizeof(T) == 4)
        return be32toh(be);
    else if constexpr (sizeof(T) == 2)
        return be16toh(be);
    else if constexpr (sizeof(T) == 1)
        return be;
    else
        static_assert(sizeof(T) && false, "Non power-of-two size");
}

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
            self.size = readbe<uint64_t>(input);
        } else if (size_be == htobe32(0)) {
            //std::cout << "size 0" << std::endl;
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

    static constexpr std::array<const char[5],19> _CONTAINERS {{
        "moov", "udta", "trak", "mdia", "meta", "ilst", "stbl", "mind", "moof", "traf",
        "minf", "cmov", "rmra", "rmda", "matt", "edts", "dinf", "stbl", "sinf"
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


namespace atom {

std::pair<uint8_t, uint32_t> read_version_flags(std::istream &stream) {
    union {
        struct {
            uint32_t flags: 24;
            uint8_t  version;
        } __attribute__((packed));
        uint32_t data;
    } u;
    u.data = readbe<uint32_t>(stream);
    return {u.version, (uint32_t)u.flags};
}

consteval uint32_t cc32(char const fourcc[4]) {
    return fourcc[0] | (fourcc[1] << 8) | (fourcc[2] << 16) | (fourcc[3] << 24);
}


struct mdhd {
    uint8_t version;
    uint32_t flags;
    uint64_t created;
    uint64_t modified;
    uint32_t timescale;
    uint64_t duration;
    uint16_t language;
    uint16_t quality;

    friend std::istream & operator >>(std::istream &stream, mdhd &self) {
        stream.read((char*)&self.version, 1);
        stream.read((char*)&self.flags, 3);
        if (self.version == 0) {
            self.created   = readbe<uint32_t>(stream);
            self.modified  = readbe<uint32_t>(stream);
            self.timescale = readbe<uint32_t>(stream);
            self.duration  = readbe<uint32_t>(stream);
            self.language  = readbe<uint16_t>(stream);
            self.quality   = readbe<uint16_t>(stream);
        } else if (self.version == 1) {
            self.created   = readbe<uint64_t>(stream);
            self.modified  = readbe<uint64_t>(stream);
            self.timescale = readbe<uint32_t>(stream);
            self.duration  = readbe<uint64_t>(stream);
            self.language  = readbe<uint16_t>(stream);
            self.quality   = readbe<uint16_t>(stream);
        }
        return stream;
    }

    uint64_t duration_s() const {
        return this->duration / this->timescale;
    }
};

struct mvhd {
    uint8_t version;
    int32_t timescale;
    uint64_t duration;

    friend std::istream & operator >>(std::istream &stream, mvhd &self) {
        stream.read((char*)&self.version, 1);
        if (self.version == 0) {
            stream.seekg(11, std::ios_base::cur);
            self.timescale = readbe<uint32_t>(stream);
            self.duration  = readbe<uint32_t>(stream);
        } else if (self.version == 1) {
            stream.seekg(19, std::ios_base::cur);
            self.timescale = readbe<uint32_t>(stream);
            self.duration  = readbe<uint64_t>(stream);
        }
        return stream;
    }
};

struct chpl_entry {
    uint64_t start;
    std::string title;

    friend std::istream &operator >>(std::istream &stream, chpl_entry &self) {
        self.start = readbe<uint64_t>(stream);
        uint8_t title_len;
        stream.read((char*)&title_len, 1);
        self.title.resize(title_len);
        stream.read(self.title.data(), title_len);
        return stream;
    }
};

struct chpl_header {
    uint8_t chapters;

    friend std::istream &operator >>(std::istream &stream, chpl_header &self) {
        stream.seekg(8, std::ios_base::cur);
        stream.read((char*)&self.chapters, 1);
        return stream;
    }
};

struct chap {
    std::vector<uint32_t> tracks;

    void read_atom(std::istream &stream, atom_header const &h) {
        int num = h.size / 4;
        tracks.resize(num);
        for (int i = 0; i < num; i++) {
            tracks[i] = readbe<uint32_t>(stream);
        }
    }
};

struct tkhd {
    uint8_t version;
    uint32_t flags;
    uint64_t created;
    uint64_t modified;
    uint32_t track_id;
    uint64_t duration;
    uint16_t layer;
    uint16_t alternate_group;
    uint16_t volume;
    uint32_t display_matrix[3][3];
    uint32_t width;
    uint32_t height;

    friend std::istream &operator >>(std::istream &stream, tkhd &self) {
        std::tie(self.version, self.flags) = read_version_flags(stream);
        if (self.version == 1) {
            self.created = readbe<uint64_t>(stream);
            self.modified = readbe<uint64_t>(stream);
        } else {
            self.created = readbe<uint32_t>(stream);
            self.modified = readbe<uint32_t>(stream);
        }
        self.track_id = readbe<uint32_t>(stream);
        stream.seekg(4, std::ios_base::cur); // reserved
        if (self.version == 1) {
            self.duration = readbe<uint64_t>(stream);
        } else {
            self.duration = readbe<uint32_t>(stream);
        }
        stream.seekg(8, std::ios_base::cur); // reserved
        self.layer          = readbe<uint16_t>(stream);
        self.alternate_group = readbe<uint16_t>(stream);
        self.volume         = readbe<uint16_t>(stream);
        stream.seekg(2, std::ios_base::cur); //reserved
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                self.display_matrix[i][j] = readbe<uint32_t>(stream);
            }
        }
        self.width      = readbe<uint32_t>(stream);
        self.height     = readbe<uint32_t>(stream);
        return stream;
    }
};

struct hdlr {
    uint8_t version;
    uint32_t flags;
    uint32_t ctype;
    uint32_t type;
    std::string title;

    void read_atom(std::istream &stream, atom_header const &h) {
        std::tie(this->version, this->flags) = read_version_flags(stream);
        this->ctype      = readbe<uint32_t>(stream);
        this->type       = readbe<uint32_t>(stream);

        readbe<uint32_t>(stream); // component manufacture
        readbe<uint32_t>(stream); // component flags
        readbe<uint32_t>(stream); // component flags mask

        auto title_size = h.size - 24;
        this->title.resize(title_size);
        stream.read(this->title.data(), title_size);
    }
};

template <typename T>
concept CustomReadAtom = requires(T &a, atom_header const &h, std::istream &s) {
    { a.read_atom(s, h) };
};

}


template <typename T>
T read_atom(std::istream &stream, const atom_header &h) {
    T data;
    stream.seekg(h.data_offset(), std::ios_base::beg);
    if constexpr (atom::CustomReadAtom<T>)
        data.read_atom(stream, h);
    else
        stream >> data;
    return data;
}

}

