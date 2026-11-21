#include <algorithm>
#include <chrono>
#include <mutex>
#include <unordered_map>

#include "routes.hpp"
#include "security.hpp"
#include "storage/access.hpp"

namespace flipkiln::control {
namespace {
std::string SetCookie(std::string_view token, bool secure) {
    return "flipkiln_session=" + std::string(token) +
           "; Path=/api; HttpOnly; SameSite=Strict; Max-Age=" + (token.empty() ? "0" : "43200") +
           (secure ? "; Secure" : "");
}

void LimitLogins(const std::string& login) {
    struct RateWindow {
        unsigned attempts{};
        std::chrono::steady_clock::time_point end;
    };
    static std::mutex mutex;
    static std::unordered_map<std::string, RateWindow> clients;
    const std::lock_guard lock(mutex);
    const auto now = std::chrono::steady_clock::now();
    if (clients.size() >= 4096) {
        std::erase_if(clients, [now](const auto& pair) { return pair.second.end < now; });
        if (clients.size() >= 4096 && !clients.contains(login)) {
            throw Problem{ProblemKind::Throttled, "rate_limited",
                          "Too many sign-in attempts. Try again in a minute."};
        }
    }
    auto& window = clients[login];
    if (window.end <= now) {
        window = {0, now + std::chrono::minutes(1)};
    }
    if (window.attempts >= 30) {
        throw Problem{ProblemKind::Throttled, "rate_limited",
                      "Too many sign-in attempts. Try again in a minute."};
    }
    ++window.attempts;
}

}

Response SessionRoute(Database& db, const Request& request, std::string_view action, bool secure_cookie,
                      const std::string& dummy_password_hash) {
    if (request.method == "POST" && action == "login") {
        const auto body = http::Body(request);
        const auto username = http::ReadId(http::Field(body, "username"));
        const auto password = http::ReadString(http::Field(body, "password"), 1, 256);
        LimitLogins(request.peer + "/" + username);
        const auto stored_hash = storage::PasswordHashFor(db, username);

        const auto& password_hash = stored_hash ? *stored_hash : dummy_password_hash;
        const bool password_matches = VerifyPassword(password, password_hash);
        if (!password_matches || !stored_hash) {
            throw Problem{ProblemKind::Unauthenticated, "invalid_credentials",
                          "Incorrect username or password."};
        }
        const auto session = storage::StartSession(db, username, request.session);
        auto response = http::Send(200, {{"username", username}});
        response.headers.emplace("Set-Cookie", SetCookie(session, secure_cookie));
        return response;
    }
    if (request.method == "GET" && action == "session") {
        return http::Send(200, {{"username", storage::Authenticate(db, request.session).username}});
    }
    if (request.method == "POST" && action == "logout") {
        http::Body(request);
        storage::EndSession(db, request.session);
        return {204, "", {{"Cache-Control", "no-store"}, {"Set-Cookie", SetCookie("", secure_cookie)}}};
    }
    Missing();
}
}
