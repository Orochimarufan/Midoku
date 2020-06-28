#include "error.h"

#include <QDebug>


namespace Midoku {

Error::Error(const QString &msg) :
    std::variant<QString, QSqlError>(msg)
{}

Error::Error(const QSqlError &db_err) :
    std::variant<QString, QSqlError>(db_err)
{}

Error::Type Error::getType() const {
    return index() == 0 ? MISC_ERROR : DB_ERROR;
}

QString Error::toString() const {
    if      (index() == 0)
        return get<0>(*this);
    else if (index() == 1)
        return get<1>(*this).text();
    return QString();
}

std::optional<QSqlError> Error::sqlError() const {
    if (index() == 1)
        return get<1>(*this);
    else
        return std::nullopt;
}

void Error::debugPrint() const {
    if (index() == 0)
        qDebug() << get<0>(*this);
    else
        qDebug() << get<1>(*this);
}

}
