#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "domain.hpp"
#include "http.hpp"

namespace flipkiln {
class StorageUnavailable final : public std::runtime_error {
public:
    StorageUnavailable();
};

class StorageClient final {
public:
    StorageClient(std::string base_url, std::string key);
    ~StorageClient();

    Response Control(const Request& request) const;
    std::optional<Json> Snapshot(std::string_view project, std::string_view environment,
                                 std::string_view token_digest) const;
    void CheckControl() const;
    void CheckRuntime() const;

private:
    class Transport;
    std::unique_ptr<Transport> transport_;
};
}
