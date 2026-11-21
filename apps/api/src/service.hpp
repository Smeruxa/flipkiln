#pragma once

#include "http.hpp"

namespace flipkiln {
class StorageClient;

enum class ApiMode { Control, Runtime };

class Service {
public:
    Service(const StorageClient& storage, ApiMode mode);
    Response Handle(const Request& request) const;

private:
    const StorageClient& storage_;
    ApiMode mode_;
};
}
