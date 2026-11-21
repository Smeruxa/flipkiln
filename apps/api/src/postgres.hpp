#pragma once

#include "db.hpp"

namespace flipkiln {
std::unique_ptr<Database> ConnectPostgres(const std::string& dsn);
}
