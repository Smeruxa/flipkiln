#include "service.hpp"

#include <iostream>

#include "control/routes.hpp"
#include "runtime/routes.hpp"
#include "security.hpp"
#include "storage/access.hpp"
#include "storage/runtime.hpp"

namespace flipkiln {
Service::Service(Database& database, bool secure_cookie, ApiMode mode)
    : database_(database),
      secure_cookie_(secure_cookie),
      mode_(mode),
      dummy_password_hash_(mode == ApiMode::Control ? HashPassword(RandomToken()) : std::string{}) {}

Response Service::Handle(const Request& request) const {
    try {
        if (request.method == "GET" && request.path == "/health/live") {
            return http::Send(200, {{"status", "ok"}});
        }
        if (request.method == "GET" && request.path == "/health/ready") {
            if (mode_ == ApiMode::Runtime) {
                storage::CheckRuntimeSchema(database_);
            } else {
                storage::CheckControlSchema(database_);
            }
            return http::Send(200, {{"status", "ok"}});
        }

        const auto path = http::SplitPath(request.path);
        if (path.size() < 3 || path.at(0) != "api") {
            Missing();
        }
        if (mode_ == ApiMode::Control) {
            if (path.at(1) == "auth" && path.size() == 3) {
                return control::SessionRoute(database_, request, path.at(2), secure_cookie_,
                                             dummy_password_hash_);
            }
            if (path.at(1) == "manage" && path.at(2) == "projects") {
                return control::ManagementRoute(database_, request, path);
            }
        } else if (path.at(1) == "client" && path.at(2) == "projects") {
            return runtime::ClientRoute(database_, request, path);
        }
        Missing();
    } catch (const Problem& problem) {
        auto response = http::Error(problem);
        if (response.status == 401 && mode_ == ApiMode::Runtime) {
            response.headers.emplace("WWW-Authenticate", "Bearer");
        }
        return response;
    } catch (const DatabaseError& error) {
        std::cerr << error.what() << '\n';
        return http::Send(503, {{"error",
                                 {{"code", "database_unavailable"},
                                  {"message", "PostgreSQL is unavailable. Try again shortly."}}}});
    } catch (const std::exception&) {
        std::cerr << "Unhandled API error\n";
        return http::Send(
            500, {{"error", {{"code", "internal_error"}, {"message", "An unexpected error occurred."}}}});
    }
}
}
