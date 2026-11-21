#pragma once

#include "db.hpp"
#include "http.hpp"

namespace flipkiln {
enum class ApiMode { Control, Runtime };

class Service {
public:
    Service(Database& database, bool secure_cookie, ApiMode mode = ApiMode::Control);
    Response Handle(const Request& request) const;

private:
    Database& database_;
    bool secure_cookie_;
    ApiMode mode_;
    std::string dummy_password_hash_;
};
}
