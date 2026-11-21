#include "routes.hpp"

#include <utility>

#include "application/runtime.hpp"
#include "internal/storage_client.hpp"
#include "security.hpp"

namespace flipkiln::runtime {
namespace {
std::string_view BearerToken(const Request& request) {
    constexpr std::string_view scheme = "Bearer ";
    const auto authorization = http::Header(request, "authorization");
    if (!http::EqualIgnoreCase(authorization.substr(0, scheme.size()), scheme)) {
        Unauthorized();
    }
    const auto token_start = authorization.find_first_not_of(' ', scheme.size());
    if (token_start == std::string_view::npos) {
        Unauthorized();
    }
    const auto token = authorization.substr(token_start);
    if (token.size() < 20 || token.size() > 256) {
        Unauthorized();
    }
    return token;
}

bool MatchesTag(std::string_view tags, std::string_view etag) {
    while (!tags.empty()) {
        const auto comma = tags.find(',');
        auto tag = http::Trim(tags.substr(0, comma));
        if (tag.starts_with("W/")) {
            tag.remove_prefix(2);
        }
        if (tag == "*" || tag == etag) {
            return true;
        }
        if (comma == std::string_view::npos) {
            break;
        }
        tags.remove_prefix(comma + 1);
    }
    return false;
}

Response ConditionalSnapshot(const Request& request, const Scope& scope, const Snapshot& snapshot) {
    const auto etag =
        "\"" + scope.project + "/" + scope.environment + "/" + std::to_string(snapshot.revision) + "\"";
    Headers headers{{"ETag", etag}, {"Cache-Control", "private, no-cache"}, {"Vary", "Authorization"}};
    if (MatchesTag(http::Header(request, "if-none-match"), etag)) {
        return {304, "", std::move(headers)};
    }
    headers.emplace("Content-Type", "application/json");
    return {200, http::SnapshotDocument(snapshot).dump(), std::move(headers)};
}
}

Response ClientRoute(const StorageClient& storage, const Request& request,
                     const std::vector<std::string>& path) {
    if (path.size() != 7 || path.at(4) != "environments") {
        Missing();
    }
    const bool read = request.method == "GET" && path.at(6) == "snapshot";
    const bool evaluate = request.method == "POST" && path.at(6) == "evaluate";
    if (!read && !evaluate) {
        Missing();
    }
    const Scope scope{path.at(3), path.at(5)};
    if (!http::ValidProject(scope.project) ||
        (scope.environment != "development" && scope.environment != "production")) {
        Unauthorized();
    }
    auto stored = storage.Snapshot(scope.project, scope.environment, Digest(BearerToken(request)));
    if (!stored) {
        Unauthorized();
    }
    const Snapshot snapshot{stored->at("revision").get<int>(), std::move(stored->at("entries"))};
    if (read) {
        return ConditionalSnapshot(request, scope, snapshot);
    }
    const auto input = http::ReadEvaluation(http::Body(request));
    return http::Send(200, application::EvaluateSnapshot(snapshot, input, scope));
}
}
