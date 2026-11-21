#include "routes.hpp"

#include "storage/runtime.hpp"

namespace flipkiln::runtime {
namespace {
std::string_view BearerToken(const Request& request) {
    const auto authorization = http::Header(request, "authorization");
    if (authorization.size() < 27 || authorization.size() > 263 ||
        !http::EqualIgnoreCase(authorization.substr(0, 7), "Bearer ")) {
        Unauthorized();
    }
    return authorization.substr(7);
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

Response ConditionalSnapshot(const Request& request, const storage::Scope& scope, const Json& snapshot) {
    const auto etag =
        "\"" + scope.project + "/" + scope.environment + "/" + snapshot.at("revision").dump() + "\"";
    Headers headers{{"ETag", etag}, {"Cache-Control", "private, no-cache"}, {"Vary", "Authorization"}};
    if (MatchesTag(http::Header(request, "if-none-match"), etag)) {
        return {304, "", std::move(headers)};
    }
    headers.emplace("Content-Type", "application/json");
    return {200, snapshot.dump(), std::move(headers)};
}
}

Response ClientRoute(Database& db, const Request& request, const std::vector<std::string>& path) {
    if (path.size() != 7 || path.at(4) != "environments") {
        Missing();
    }
    const bool read = request.method == "GET" && path.at(6) == "snapshot";
    const bool evaluate = request.method == "POST" && path.at(6) == "evaluate";
    if (!read && !evaluate) {
        Missing();
    }
    const storage::Scope scope{path.at(3), path.at(5)};
    const auto snapshot = storage::AuthorizedSnapshot(db, scope, BearerToken(request));
    if (read) {
        return ConditionalSnapshot(request, scope, snapshot);
    }
    const auto results =
        http::EvaluateBody(snapshot.at("entries"), http::Body(request), scope.project, scope.environment);
    return http::Send(200, {{"revision", snapshot.at("revision")}, {"results", results}});
}
}
