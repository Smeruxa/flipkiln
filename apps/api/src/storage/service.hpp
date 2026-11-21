#pragma once

#include "db.hpp"
#include "http.hpp"

namespace flipkiln::storage {
class Service {
public:
    Service(Database& control, Database& reader, bool secure_cookie, std::string_view control_key,
            std::string_view runtime_key);
    Response Handle(const Request& request) const;

private:
    Response Control(const Request& request) const;
    Response Runtime(const Request& request) const;

    Database& control_;
    Database& reader_;
    bool secure_cookie_;
    std::string control_key_digest_;
    std::string runtime_key_digest_;
    std::string dummy_password_hash_;
};
}
