#pragma once

#include "database.h"

#include <filesystem>
#include <set>
#include <unordered_set>

#include <QObject>
#include <QSet>
#include <QMimeType>


namespace Midoku::Library {

/**
 * @brief The Importer class
 * Import new audiobooks from paths
 */
class Importer : public QObject
{
    Q_OBJECT

    Database &db;

    DBResult<std::unordered_set<std::string>> inventory();

    DBResult<void> process_dir(const std::set<std::filesystem::path> &files);

public:
    static const QSet<QString> supported_types;

    explicit Importer(Database &db);

signals:
    void progress(int current, int max, const QString &message);
    void finished();

public slots:
    DBResult<void> import(const std::vector<std::filesystem::path> &paths);
};

}
