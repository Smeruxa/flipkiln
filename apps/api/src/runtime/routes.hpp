#pragma once

#include "http.hpp"

namespace flipkiln {
class StorageClient;

namespace runtime {
Response ClientRoute(const StorageClient& storage, const Request& request,
                     const std::vector<std::string>& path);
}
}
