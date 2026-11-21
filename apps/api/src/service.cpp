#include "service.hpp"

#include <iostream>

#include "internal/storage_client.hpp"
#include "runtime/routes.hpp"

namespace flipkiln {
Service::Service(const StorageClient& storage, ApiMode mode) : storage_(storage), mode_(mode) {}

Response Service::Handle(const Request& request) const {
    try {
        if (request.method == "GET" && request.path == "/health/live") {
            return http::Send(200, {{"status", "ok"}});
        }
        if (request.method == "GET" && request.path == "/health/ready") {
            if (mode_ == ApiMode::Runtime) {
                storage_.CheckRuntime();
            } else {
                storage_.CheckControl();
            }
            return http::Send(200, {{"status", "ok"}});
        }

        const auto path = http::SplitPath(request.path);
        if (path.size() < 3 || path.at(0) != "api") {
            Missing();
        }
        if (mode_ == ApiMode::Control) {
            if ((path.at(1) == "auth" && path.size() == 3) ||
                (path.at(1) == "manage" && path.at(2) == "projects")) {
                return storage_.Control(request);
            }
        } else if (path.at(1) == "client" && path.at(2) == "projects") {
            return runtime::ClientRoute(storage_, request, path);
        }
        Missing();
    } catch (const Problem& problem) {
        auto response = http::Error(problem);
        if (response.status == 401 && mode_ == ApiMode::Runtime) {
            response.headers.emplace("WWW-Authenticate", "Bearer realm=\"flipkiln\"");
        }
        return response;
    } catch (const StorageUnavailable& error) {
        return http::Send(503, {{"error", {{"code", "storage_unavailable"}, {"message", error.what()}}}});
    } catch (const std::exception&) {
        std::cerr << "Unhandled API error\n";
        return http::Send(
            500, {{"error", {{"code", "internal_error"}, {"message", "An unexpected error occurred."}}}});
    }
}
}
