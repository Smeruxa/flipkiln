#pragma once

#include "db.hpp"
#include "http.hpp"

namespace flipkiln::control {
Response SessionRoute(Database& db, const Request& request, std::string_view action, bool secure_cookie,
                      const std::string& dummy_password_hash);
Response ManagementRoute(Database& db, const Request& request, const std::vector<std::string>& path);
}
