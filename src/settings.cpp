#include "settings.h"

namespace Midoku {

Settings::Settings()
{

}

QStringList Settings::getLibraryPaths() {
    QSettings s;
    return s.value("library/paths").toStringList();
}

void Settings::setLibraryPaths(const QStringList &sl) {
    QSettings s;
    s.setValue("library/paths", QVariant::fromValue(sl));
    emit libraryPathsChanged();
}

std::vector<std::filesystem::path> Settings::getLibraryPaths2() {
    std::vector<std::filesystem::path> ps;
    for (auto &path : getLibraryPaths())
        ps.emplace_back(path.toStdString());
    return ps;
}

}
