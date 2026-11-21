#include "service.hpp"

#include <openssl/crypto.h>

#include <algorithm>
#include <iostream>

#include "access.hpp"
#include "control/routes.hpp"
#include "runtime.hpp"
#include "security.hpp"

namespace flipkiln::storage {
namespace {
constexpr std::string_view kControlPath = "/internal/v1/control";
constexpr std::string_view kControlPrefix = "/internal/v1/control/";
constexpr std::string_view kRuntimePrefix = "/internal/v1/runtime/";

std::string KeyDigest(std::string_view key) {
    if (key.size() < 32 || key.size() > 256 ||
        !std::ranges::all_of(key, [](unsigned char byte) { return byte >= 33 && byte <= 126; })) {
        throw std::runtime_error("Storage credentials must contain 32–256 visible ASCII characters");
    }
    return Digest(key);
}

void RequireKey(const Request& request, std::string_view expected_digest) {
    const auto key = http::Header(request, "x-flipkiln-storage-key");
    if (key.size() >= 32 && key.size() <= 256) {
        const auto actual_digest = Digest(key);
        if (CRYPTO_memcmp(actual_digest.data(), expected_digest.data(), actual_digest.size()) == 0) {
            return;
        }
    }
    throw Problem{ProblemKind::Forbidden, "storage_forbidden", "Invalid storage credential."};
}

Response Health() { return http::Send(200, {{"status", "ok"}}); }

Scope ReadScope(const Json& body) {
    auto project = http::ReadString(http::Field(body, "project"), 1, 63);
    if (!http::ValidProject(project)) {
        Bad("Invalid project identifier.");
    }
    auto environment = http::ReadString(http::Field(body, "environment"), 1, 11);
    if (environment != "development" && environment != "production") {
        Bad("Invalid environment.");
    }
    return {std::move(project), std::move(environment)};
}

std::string ReadTokenDigest(const Json& body) {
    auto digest = http::ReadString(http::Field(body, "tokenDigest"), 64, 64);
    const auto hexadecimal = [](char byte) {
        return (byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f');
    };
    if (!std::ranges::all_of(digest, hexadecimal)) {
        Bad("Invalid token digest.");
    }
    return digest;
}
}

Service::Service(Database& control, Database& reader, bool secure_cookie, std::string_view control_key,
                 std::string_view runtime_key)
    : control_(control),
      reader_(reader),
      secure_cookie_(secure_cookie),
      control_key_digest_(KeyDigest(control_key)),
      runtime_key_digest_(KeyDigest(runtime_key)),
      dummy_password_hash_(HashPassword(RandomToken())) {
    if (control_key_digest_ == runtime_key_digest_) {
        throw std::runtime_error("Storage control and runtime credentials must differ");
    }
}

Response Service::Control(const Request& request) const {
    RequireKey(request, control_key_digest_);
    if (request.method == "GET" && request.path == "/internal/v1/control/ready") {
        CheckControlSchema(control_);
        return Health();
    }

    const auto public_path = std::string_view(request.path).substr(kControlPath.size());
    const auto path = http::SplitPath(public_path);
    if (path.size() < 3 || path.at(0) != "api") {
        Missing();
    }
    if (path.size() == 3 && path.at(1) == "auth") {
        const auto peer_header = http::Header(request, "x-flipkiln-client-peer");
        const auto peer = peer_header.empty() || peer_header.size() > 128 ? request.peer : peer_header;
        return control::SessionRoute(control_, request, path.at(2), secure_cookie_, dummy_password_hash_,
                                     peer);
    }
    if (path.at(1) == "manage" && path.at(2) == "projects") {
        return control::ManagementRoute(control_, request, path);
    }
    Missing();
}

Response Service::Runtime(const Request& request) const {
    RequireKey(request, runtime_key_digest_);
    if (request.method == "GET" && request.path == "/internal/v1/runtime/ready") {
        CheckRuntimeSchema(reader_);
        return Health();
    }
    if (request.method != "POST" || request.path != "/internal/v1/runtime/snapshot") {
        Missing();
    }

    const auto body = http::Body(request);
    if (body.size() != 3) {
        Bad("Expected only project, environment and tokenDigest.");
    }
    const auto scope = ReadScope(body);
    const auto digest = ReadTokenDigest(body);
    const auto snapshot = FindClientSnapshot(reader_, scope, digest);
    return http::Send(200, {{"snapshot", snapshot ? http::SnapshotDocument(*snapshot) : Json(nullptr)}});
}

Response Service::Handle(const Request& request) const {
    try {
        if (request.method == "GET" && request.path == "/health/live") {
            return Health();
        }
        if (request.method == "GET" && request.path == "/health/ready") {
            CheckControlSchema(control_);
            CheckRuntimeSchema(reader_);
            return Health();
        }
        if (request.path.starts_with(kControlPrefix)) {
            return Control(request);
        }
        if (request.path.starts_with(kRuntimePrefix)) {
            return Runtime(request);
        }
        Missing();
    } catch (const Problem& problem) {
        return http::Error(problem);
    } catch (const DatabaseError& error) {
        std::cerr << error.what() << '\n';
        return http::Send(503, {{"error",
                                 {{"code", "database_unavailable"},
                                  {"message", "PostgreSQL is unavailable. Try again shortly."}}}});
    } catch (const std::exception&) {
        std::cerr << "Unhandled storage API error\n";
        return http::Send(
            500, {{"error", {{"code", "internal_error"}, {"message", "An unexpected error occurred."}}}});
    }
}
}
