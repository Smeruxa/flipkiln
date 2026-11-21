#include <drogon/drogon.h>
#include <trantor/net/InetAddress.h>

#include <condition_variable>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "config.hpp"
#include "internal/storage_client.hpp"
#include "migrations.hpp"
#include "postgres.hpp"
#include "service.hpp"
#include "storage/service.hpp"

namespace {
constexpr std::size_t kRequestBodyLimit = 1024 * 1024;

using Callback = std::function<void(const drogon::HttpResponsePtr&)>;
using Handler = std::function<flipkiln::Response(const flipkiln::Request&)>;

class Workers {
public:
    explicit Workers(unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            threads_.emplace_back([this](std::stop_token stop) { Run(stop); });
        }
    }

    ~Workers() {
        {
            const std::lock_guard lock(mutex_);
            done_ = true;
            queue_.clear();
        }
        pending_.notify_all();
    }

    bool Enqueue(std::function<void()> work) {
        {
            const std::lock_guard lock(mutex_);
            if (done_ || queue_.size() >= 64) {
                return false;
            }
            queue_.push_back(std::move(work));
        }
        pending_.notify_one();
        return true;
    }

private:
    void Run(std::stop_token stop) {
        while (true) {
            std::function<void()> next;
            {
                std::unique_lock lock(mutex_);
                pending_.wait(lock, stop, [this] { return done_ || !queue_.empty(); });
                if (queue_.empty() || stop.stop_requested()) {
                    return;
                }
                next = std::move(queue_.front());
                queue_.pop_front();
            }
            next();
        }
    }

    std::mutex mutex_;
    std::condition_variable_any pending_;
    std::deque<std::function<void()>> queue_;
    bool done_ = false;
    std::vector<std::jthread> threads_;
};

drogon::HttpResponsePtr ToHttp(flipkiln::Response result, bool forwarded_head = false) {
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.status));
    response->setContentTypeCode(drogon::CT_NONE);
    response->setPassThrough(forwarded_head);
    for (const auto& [name, value] : result.headers) {
        if (!forwarded_head && name == "Content-Type" && value == "application/json") {
            response->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        } else {
            response->addHeader(name, value);
        }
    }
    if (!result.body.empty()) {
        response->setBody(std::move(result.body));
    }
    return response;
}

void RunServer(const Handler& handle, std::string_view mode) {
    auto& app = drogon::app();
    Workers workers(8);
    Workers readiness(1);
    const auto routes =
        mode == "storage" ? R"(^/(?:internal/v1/.*|health/.*)$)" : R"(^/(?:api(?:/.*)?|health/.*)$)";
    const bool control_gateway = mode == "control";
    app.disableSession()
        .enableGzip(false)
        .setDocumentRoot("/usr/local/share/flipkiln/empty")
        .setUploadPath("/tmp/flipkiln-upload")
        .setThreadNum(2)
        .setClientMaxBodySize(2 * kRequestBodyLimit)
        .setClientMaxMemoryBodySize(2 * kRequestBodyLimit)
        .addListener("0.0.0.0", 8080)
        .registerHandlerViaRegex(routes, [handle, &workers, &readiness, control_gateway](
                                             const drogon::HttpRequestPtr& request, Callback&& respond) {
            const auto body = request->body();
            if (body.size() > kRequestBodyLimit) {
                const flipkiln::Problem problem{flipkiln::ProblemKind::PayloadTooLarge, "body_too_large",
                                                "The request exceeds 1 MiB."};
                respond(ToHttp(flipkiln::http::Error(problem)));
                return;
            }

            flipkiln::Request input;
            input.path = request->path();
            input.method = control_gateway && input.path.starts_with("/api/") && request->isHead()
                               ? "HEAD"
                               : request->getMethodString();
            input.body = body;
            input.peer = request->peerAddr().toIp();
            input.session = request->getCookie("flipkiln_session");
            for (const auto& [name, value] : request->getHeaders()) {
                input.headers.emplace(name, value);
            }
            if (input.path == "/health/live") {
                respond(ToHttp(handle(input)));
                return;
            }
            const bool ready = input.path == "/health/ready" || input.path == "/internal/v1/control/ready" ||
                               input.path == "/internal/v1/runtime/ready";
            auto& pool = ready ? readiness : workers;
            if (!pool.Enqueue([handle, input = std::move(input), respond] {
                    respond(ToHttp(handle(input), input.method == "HEAD"));
                })) {
                respond(
                    ToHttp({503,
                            R"({"error":{"code":"busy","message":"The server is busy. Try again shortly."}})",
                            {{"Cache-Control", "no-store"}, {"Content-Type", "application/json"}}}));
            }
        });
    std::cerr << "Flipkiln " << mode << " API listening on port 8080\n";
    app.run();
}

std::string RequiredEnv(const char* name) {
    auto value = flipkiln::Env(name);
    if (value.empty()) {
        throw std::runtime_error(std::string(name) + " is required");
    }
    return value;
}

void Serve() {
    const auto mode = flipkiln::Env("FLIPKILN_API_MODE", "control");
    drogon::app().setLogLevel(trantor::Logger::kWarn);
    if (mode == "storage") {
        const auto control_dsn = RequiredEnv("DATABASE_URL");
        const auto reader_dsn = RequiredEnv("FLIPKILN_STORAGE_READER_DATABASE_URL");
        const auto control_key = RequiredEnv("FLIPKILN_STORAGE_CONTROL_KEY");
        const auto runtime_key = RequiredEnv("FLIPKILN_STORAGE_RUNTIME_KEY");
        const auto control = flipkiln::ConnectPostgres(control_dsn);
        const auto reader = flipkiln::ConnectPostgres(reader_dsn);
        const auto secure_cookie = flipkiln::Env("FLIPKILN_COOKIE_SECURE", "true") != "false";
        const flipkiln::storage::Service storage(*control, *reader, secure_cookie, control_key, runtime_key);
        RunServer([&storage](const flipkiln::Request& request) { return storage.Handle(request); }, mode);
        return;
    }
    if (mode != "control" && mode != "runtime") {
        throw std::runtime_error("FLIPKILN_API_MODE must be control, runtime, or storage");
    }
    const flipkiln::StorageClient storage(RequiredEnv("FLIPKILN_STORAGE_URL"),
                                          RequiredEnv("FLIPKILN_STORAGE_KEY"));
    const flipkiln::Service gateway(
        storage, mode == "control" ? flipkiln::ApiMode::Control : flipkiln::ApiMode::Runtime);
    RunServer([&gateway](const flipkiln::Request& request) { return gateway.Handle(request); }, mode);
}
}

int main(int argc, char** argv) {
    try {
        if (argc == 1) {
            Serve();
        } else if (argc == 2 && std::string_view(argv[1]) == "migrate") {
            flipkiln::Migrate();
        } else if (argc == 3 && std::string_view(argv[1]) == "create-user") {
            std::string password;
            std::getline(std::cin, password);
            flipkiln::CreateUser(argv[2], password);
        } else {
            std::cerr << "Usage: flipkiln-api [migrate | create-user USERNAME]\n";
            return 2;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
