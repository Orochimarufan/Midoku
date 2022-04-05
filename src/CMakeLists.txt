
set(HEADERS
    "util/result.h"
    "util/enumerate.h"
    "util/type_sequence.h"
    "util/tuple.h"
    "util/tuple_util.h"
    "util/tstring.h"
    "util/orm.h"
    "mpv/mpv.h"
    "mpv/mpv_type.h"
    "mpv/mpvframebuffer.h"
    "library/database.h"
    "library/book.h"
    "library/blob.h"
    "library/chapter.h"
    "library/progress.h"
    "library/schema.h"
    "library/models.h"
    "library/importer.h"
    "library/importer_metadata.h"
    "logic/error.h"
    "logic/player.h"
    "logic/app.h"
    "logic/mpris.h"
    "settings.h"
    )

set(SOURCES
    "main.cpp"
    "mpv/mpv.cpp"
    "mpv/mpvframebuffer.cpp"
    "library/database.cpp"
    "library/schema.cpp"
    "library/models.cpp"
    "library/book.cpp"
    "library/blob.cpp"
    "library/chapter.cpp"
    "library/progress.cpp"
    "library/importer.cpp"
    "library/importer_metadata.cpp"
    "library/importer_isobmff.cpp"
    "logic/error.cpp"
    "logic/player.cpp"
    "logic/app.cpp"
    "logic/mpris.cpp"
    "settings.cpp"
    "qml.qrc"
    )

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Core Qt5::Quick Qt5::Sql Qt5::DBus mpv PkgConfig::TagLib stdc++fs Qt5::Widgets)
