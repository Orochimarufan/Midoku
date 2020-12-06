#include "importer_metadata.h"
#include "util/isobmff.h"

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
// Metadata
static bool isobmff_chapters(ImportState &import, const std::filesystem::path &p) {
    using namespace isobmff;

    auto fs = std::ifstream(p, std::ios_base::binary);

    // Find relevant Atoms
    atom_header mdhd_atom;
    atom_header mvhd_atom;
    atom_header chpl_atom;

    for (auto a = AtomParser(fs); a; ++a) {
        if (a->is("moov")) {
            for (auto b = a.children(); b; ++b) {
                if (b->is("mvhd")) {
                    mvhd_atom = b.current();
                } else if (b->is("mdhd")) {
                    mdhd_atom = b.current();
                } else if (b->is("udta")) {
                    for (auto c = b.children(); c; ++c) {
                        if (c->is("chpl")) {
                            chpl_atom = c.current();
                            break;
                        }
                    }
                }
            }
        }
    }

    // --- Duration ---
    if (mdhd_atom) {
        auto mdhd = read_atom<atom::mdhd>(fs, mdhd_atom);
        import.length = mdhd.duration_s();
    } else if (mvhd_atom) {
        auto mvhd = read_atom<atom::mvhd>(fs, mvhd_atom);
        import.length = mvhd.duration / mvhd.timescale;
    }

    qDebug() << "MP4 Duration: " << import.length;

    // --- Chapters ---
    if (!mvhd_atom || !chpl_atom) {
        std::cerr << "ISOBMFF/M4B: No mvhd and/or chpl found" << std::endl;
        return false;
    }

    // Parse mvhd
    auto mvhd = read_atom<atom::mvhd>(fs, mvhd_atom);
    auto scale = mvhd.timescale * 10000;

    // Parse chpl
    auto chpl = read_atom<atom::chpl_header>(fs, chpl_atom);
    auto entry = atom::chpl_entry{};
    for (int i = 1; i <= chpl.chapters; i++) {
        fs >> entry;
        import.chaps.emplace(i, ChapterInfo{.no=i, .start=(int64_t)(entry.start/scale), .name=QString::fromStdString(entry.title)});
        qDebug() << "MP4 Chapter: " << entry.start / scale << ":" << entry.title.c_str();
    }

    // TODO add quicktime chapter (CHAP + TRAK) support
    // probably going to be painful.

    return !import.chaps.empty();
}

MediaInfo isobmff_process(const std::filesystem::path &file, const QMimeType &type) {
    Q_UNUSED(type)
    ImportState s(file);

    // TagLib
    {
        TagLib::MP4::File isobmf(file.c_str());
        // Get duration from isombff_chapters() later

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
