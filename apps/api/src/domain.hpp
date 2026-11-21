#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace flipkiln {
using Json = nlohmann::json;

struct Scope {
    std::string project;
    std::string environment;
};

struct Snapshot {
    int revision;
    Json entries;
};

struct EvaluationInput {
    Json keys;
    Json context;
};

Json ValidateEntries(const Json& entries);
bool ValidIdentifier(std::string_view value);
bool ValidContext(const Json& context);
Json Evaluate(const Json& entries, const Json& keys, const Json& context, std::string_view project,
              std::string_view environment);
std::uint64_t Bucket(std::string_view project, std::string_view environment, std::string_view key,
                     std::string_view rule, std::string_view id);
}
