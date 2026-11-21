#include "http.hpp"

#include <algorithm>
#include <limits>

namespace flipkiln::http {
namespace {
int Status(ProblemKind kind) {
    switch (kind) {
        case ProblemKind::Invalid:
            return 400;
        case ProblemKind::Unauthenticated:
            return 401;
        case ProblemKind::Forbidden:
            return 403;
        case ProblemKind::Missing:
            return 404;
        case ProblemKind::Conflict:
            return 409;
        case ProblemKind::InvalidEntries:
            return 422;
        case ProblemKind::Throttled:
            return 429;
        case ProblemKind::PayloadTooLarge:
            return 413;
        case ProblemKind::UnsupportedMedia:
            return 415;
    }
    return 500;
}
}

Response Error(const Problem& problem) {
    auto detail = Json{{"code", problem.code}, {"message", problem.message}};
    if (!problem.issues.is_null()) {
        detail["issues"] = problem.issues;
    }
    return Send(Status(problem.kind), {{"error", detail}});
}

void RequireSameSite(const Request& request) {
    if (Header(request, "sec-fetch-site") == "cross-site") {
        throw Problem{ProblemKind::Forbidden, "forbidden", "Cross-site mutations are blocked."};
    }
}

Response Send(int status, const Json& json) {
    return {status, json.dump(), {{"Cache-Control", "no-store"}, {"Content-Type", "application/json"}}};
}

std::string_view Header(const Request& request, const std::string& name) {
    const auto found = request.headers.find(name);
    return found == request.headers.end() ? std::string_view{} : std::string_view(found->second);
}

std::string_view Trim(std::string_view value) {
    const auto start = value.find_first_not_of(" \t");
    if (start == std::string_view::npos) {
        return {};
    }
    return value.substr(start, value.find_last_not_of(" \t") + 1 - start);
}

bool EqualIgnoreCase(std::string_view left, std::string_view right) {
    const auto ascii_lower = [](char ch) { return ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch; };
    return std::ranges::equal(left, right, std::ranges::equal_to{}, ascii_lower, ascii_lower);
}

std::vector<std::string> SplitPath(std::string_view path) {
    std::vector<std::string> parts;
    while (!path.empty() && path.front() == '/') {
        path.remove_prefix(1);
        const auto slash = path.find('/');
        parts.emplace_back(path.substr(0, slash));
        if (slash == std::string_view::npos) {
            break;
        }
        path.remove_prefix(slash);
    }
    return parts;
}

const Json& Field(const Json& value, const char* name) {
    const auto found = value.find(name);
    if (found == value.end()) {
        Bad("A required field is missing.");
    }
    return *found;
}

std::string ReadString(const Json& value, std::size_t low, std::size_t high) {
    if (!value.is_string()) {
        Bad("Expected a string field.");
    }
    const auto content = value.get<std::string>();
    if (content.size() < low || content.size() > high || content.find('\0') != std::string::npos) {
        Bad("A string field is outside its allowed length.");
    }
    return content;
}

int ReadInt(const Json& value) {
    if (!value.is_number_integer() || value < 1 || value > std::numeric_limits<std::int32_t>::max()) {
        Bad("Expected a positive integer.");
    }
    return value.get<int>();
}

std::string ReadId(const Json& value) {
    const auto id = ReadString(value, 1, 64);
    if (!ValidIdentifier(id)) {
        Bad("Use an identifier beginning with a lowercase letter.");
    }
    return id;
}

bool ValidProject(std::string_view project) {
    return project.size() <= 63 && ValidIdentifier(project) &&
           project.find_first_of("_.") == std::string_view::npos;
}

Json Body(const Request& request) {
    const auto content_type = Header(request, "content-type");
    if (!EqualIgnoreCase(Trim(content_type.substr(0, content_type.find(';'))), "application/json")) {
        throw Problem{ProblemKind::UnsupportedMedia, "unsupported_media_type",
                      "Send JSON with Content-Type application/json."};
    }
    if (request.body.size() > 1048576) {
        throw Problem{ProblemKind::PayloadTooLarge, "body_too_large", "The request exceeds 1 MiB."};
    }
    RequireSameSite(request);

    const auto document = Json::parse(
        request.body,
        [](int depth, Json::parse_event_t, Json&) {
            if (depth > 64) {
                Bad("The request exceeds 64 levels of JSON nesting.");
            }
            return true;
        },
        false);
    if (!document.is_object()) {
        Bad("Send a valid JSON object.");
    }
    return document;
}

Json EvaluateBody(const Json& entries, const Json& body, std::string_view project,
                  std::string_view environment) {
    const auto& keys = Field(body, "keys");
    const auto& context = Field(body, "context");
    if (!keys.is_array() || keys.empty() || keys.size() > 100) {
        Bad("Request between 1 and 100 keys.");
    }
    if (!ValidContext(context)) {
        Bad("Send a valid context object.");
    }
    const auto expected = body.value("expectedTypes", Json::object());
    if (!expected.is_object() || expected.size() > 100) {
        Bad("Expected a map of keys and types.");
    }
    for (const auto& [key, type] : expected.items()) {
        if (!ValidIdentifier(key) ||
            (type != "boolean" && type != "string" && type != "number" && type != "json")) {
            Bad("Expected type must be boolean, string, number, or json.");
        }
    }
    auto typed_keys = Json::array();
    for (const auto& key : keys) {
        const auto identifier = ReadId(key);
        if (expected.contains(identifier)) {
            typed_keys.push_back({{"key", identifier}, {"type", expected.at(identifier)}});
        } else {
            typed_keys.push_back(identifier);
        }
    }
    return Evaluate(entries, typed_keys, context, project, environment);
}

}
