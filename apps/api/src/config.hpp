#pragma once

#include <cstdlib>
#include <string>
#include <string_view>

namespace flipkiln {
inline std::string Env(const char* key, std::string_view fallback = "") {
    const char* value = std::getenv(key);
    return value ? std::string(value) : std::string(fallback);
}
}
