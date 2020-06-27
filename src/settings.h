#pragma once

#include <vector>
#include <filesystem>

#include <QObject>
#include <QSettings>
#include <QString>


namespace Midoku {

class Settings : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QStringList libraryPaths READ getLibraryPaths WRITE setLibraryPaths)

public:
    Settings();

    QStringList getLibraryPaths();
    void setLibraryPaths(const QStringList &);

    std::vector<std::filesystem::path> getLibraryPaths2();

signals:
    void libraryPathsChanged();
};

}
