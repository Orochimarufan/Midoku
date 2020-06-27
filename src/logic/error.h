#pragma once

#include "util/result.h"

#include <optional>
#include <variant>

#include <QSqlError>


namespace Midoku {

class Error : std::variant<QString, QSqlError>{
public:
    enum Type {
        MISC_ERROR,
        DB_ERROR
    };

    Error(const QString &);
    Error(const QSqlError &);

    Error(const Error &) = default;
    Error(Error &&) = default;

    Type getType() const;

    QString toString() const;

    std::optional<QSqlError> sqlError() const;
};


template <typename R>
using Result = Util::Result<R, Error>;

}
