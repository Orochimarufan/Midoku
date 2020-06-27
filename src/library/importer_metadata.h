#pragma once

#include <unordered_map>
#include <filesystem>

#include <QString>
#include <QImage>
#include <QMimeDatabase>

#include <taglib/audioproperties.h>
#include <taglib/tstringlist.h>


namespace Midoku::Library {

// Metadata
struct ChapterInfo {
    int no = 0;
    // times in sec. TODO: what about fractions?
    int64_t start = 0;
    int64_t end = -1;
    int64_t length = -1;
    QString name;

    int64_t get_length() const {
        return length > -1? length : (end - start);
    }

    int64_t get_end() const {
        return end > -1? end : start + length;
    }
};

struct MediaInfo {
    QString media;
    QString title;
    QString author;
    QString reader;
    bool multi_chapter = false;
    std::vector<ChapterInfo> chapters;
    QImage cover;
};

// Guesswork
inline QString guess_title_from_directory(const std::filesystem::path &file) {
    return QString::fromStdString(file.parent_path().filename());
}

inline QString guess_title_from_filename(const std::filesystem::path &file) {
    return QString::fromStdString(file.stem());
}

inline QString tqstr(const TagLib::StringList &sl) {
    if (sl.size() < 1)
        return QString();
    else {
        auto& s = sl.back();
        return QString::fromWCharArray(s.toCWString(), s.size());
    }
}


// Importer
struct ImportState {
    const std::filesystem::path &path;
    MediaInfo info;
    int64_t length;
    QString artist;
    QString album;
    QString title;
    QString track;
    std::unordered_map<int, ChapterInfo> chaps;

    inline ImportState(const std::filesystem::path &file) :
        path(file),
        info{.media = QString::fromStdString(file.native())}
    {}

    inline void readAudioProperties(TagLib::AudioProperties *p) {
        length = p->lengthInSeconds();
    }

    inline bool handleCommonTags(const TagLib::String &tag, const TagLib::StringList &value) {
        if      (tag == "AUTHOR")
            info.author = tqstr(value);
        else if (tag == "PERFORMER")
            info.reader = tqstr(value);
        else if (tag == "ARTIST")
            artist = tqstr(value);
        else if (tag == "ALBUM")
            album = tqstr(value);
        else if (tag == "TITLE")
            title = tqstr(value);
        else if (tag == "TRACKNUMBER")
            track = tqstr(value);
        else
            return false;
        return true;
    }

    bool finalize();
};

// Simple MP3, OGG Vorbis/Opus/FLAC
MediaInfo mpeg_process(const std::filesystem::path &file);
MediaInfo ogg_process(const std::filesystem::path &file, const QMimeType &type);

// ISOBMFF MP4/M4A/M4B
MediaInfo isobmff_process(const std::filesystem::path &file, const QMimeType &type);

}
