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
#include "migrations.hpp"
#include "postgres.hpp"
#include "service.hpp"

namespace {
using Callback = std::function<void(const drogon::HttpResponsePtr&)>;

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

drogon::HttpResponsePtr ToHttp(flipkiln::Response result) {
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.status));
    response->setContentTypeCode(drogon::CT_NONE);
    for (const auto& [name, value] : result.headers) {
        if (name == "Content-Type" && value == "application/json") {
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

void Serve() {
    const auto dsn = flipkiln::Env("DATABASE_URL");
    if (dsn.empty()) {
        throw std::runtime_error("DATABASE_URL is required");
    }
    const auto api_mode = flipkiln::Env("FLIPKILN_API_MODE", "control");
    if (api_mode != "control" && api_mode != "runtime") {
        throw std::runtime_error("FLIPKILN_API_MODE must be control or runtime");
    }
    const auto mode = api_mode == "runtime" ? flipkiln::ApiMode::Runtime : flipkiln::ApiMode::Control;
    auto& app = drogon::app();
    app.setLogLevel(trantor::Logger::kWarn);
    auto db = flipkiln::ConnectPostgres(dsn);
    const flipkiln::Service service(*db, flipkiln::Env("FLIPKILN_COOKIE_SECURE", "true") != "false", mode);
    Workers workers(8);
    Workers readiness(1);
    app.disableSession()
        .enableGzip(false)
        .setDocumentRoot("/usr/local/share/flipkiln/empty")
        .setUploadPath("/tmp/flipkiln-upload")
        .setThreadNum(2)
        .setClientMaxBodySize(1024 * 1024)
        .setClientMaxMemoryBodySize(1024 * 1024)
        .addListener("0.0.0.0", 8080)
        .registerHandlerViaRegex(
            R"(^/(?:api(?:/.*)?|health/.*)$)",
            [&service, &workers, &readiness](const drogon::HttpRequestPtr& request, Callback&& respond) {
                flipkiln::Request input;
                input.method = request->getMethodString();
                input.path = request->path();
                input.body = request->body();
                input.peer = request->peerAddr().toIp();
                input.session = request->getCookie("flipkiln_session");
                for (const auto& [name, value] : request->getHeaders()) {
                    input.headers.emplace(name, value);
                }
                if (input.path == "/health/live") {
                    respond(ToHttp(service.Handle(input)));
                    return;
                }
                auto& pool = input.path == "/health/ready" ? readiness : workers;
                if (!pool.Enqueue([&service, input = std::move(input), respond] {
                        respond(ToHttp(service.Handle(input)));
                    })) {
                    respond(ToHttp(
                        {503,
                         R"({"error":{"code":"busy","message":"The server is busy. Try again shortly."}})",
                         {{"Cache-Control", "no-store"}, {"Content-Type", "application/json"}}}));
                }
            });
    std::cerr << "Flipkiln " << api_mode << " API listening on port 8080\n";
    app.run();
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
