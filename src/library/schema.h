#pragma once

#include "database.h"


namespace Midoku::Library {

DBResult<void> upgrade_schema(Database *db);

}
