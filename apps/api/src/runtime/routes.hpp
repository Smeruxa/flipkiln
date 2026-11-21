#pragma once

#include "db.hpp"
#include "http.hpp"

namespace flipkiln::runtime {
Response ClientRoute(Database& db, const Request& request, const std::vector<std::string>& path);
}
