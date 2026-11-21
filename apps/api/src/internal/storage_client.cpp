#include "storage_client.hpp"

#include <arpa/inet.h>
#include <drogon/HttpClient.h>
#include <trantor/net/EventLoopThread.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <map>
#include <mutex>
#include <thread>
#include <utility>

namespace flipkiln {
namespace {
constexpr double kTimeout = 6.0;
constexpr double kReadinessTimeout = 1.5;
constexpr char kSession[] = "flipkiln_session";

bool IsIp(std::string_view host, int family) {
    std::array<unsigned char, 16> address{};
    return inet_pton(family, std::string(host).c_str(), address.data()) == 1;
}

bool Alphanumeric(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
}

bool ValidHost(std::string_view host) {
    if (host.ends_with('.')) {
        host.remove_suffix(1);
    }
    if (host.empty() || host.size() > 253 || host.ends_with('.')) {
        return false;
    }
    while (!host.empty()) {
        const auto dot = host.find('.');
        const auto label = host.substr(0, dot);
        if (label.empty() || label.size() > 63 || !Alphanumeric(label.front()) ||
            !Alphanumeric(label.back()) ||
            !std::ranges::all_of(label, [](char ch) { return Alphanumeric(ch) || ch == '-'; })) {
            return false;
        }
        if (dot == std::string_view::npos) {
            return true;
        }
        host.remove_prefix(dot + 1);
    }
    return false;
}

bool ValidPort(std::string_view port) {
    unsigned number = 0;
    const auto [end, error] = std::from_chars(port.data(), port.data() + port.size(), number);
    return error == std::errc{} && end == port.data() + port.size() && number > 0 && number <= 65535;
}

std::string_view Authority(std::string_view url) {
    auto authority = url.substr(url.find("://") + 3);
    if (authority.ends_with('/')) {
        authority.remove_suffix(1);
    }
    return authority;
}

bool ValidOrigin(std::string_view url) {
    if ((!url.starts_with("https://") && !url.starts_with("http://")) || url.size() > 270 ||
        url.find('\0') != std::string_view::npos) {
        return false;
    }
    const auto authority = Authority(url);
    if (authority.starts_with('[')) {
        const auto bracket = authority.find(']');
        if (bracket == std::string_view::npos || !IsIp(authority.substr(1, bracket - 1), AF_INET6)) {
            return false;
        }
        const auto suffix = authority.substr(bracket + 1);
        return suffix.empty() || (suffix.starts_with(':') && ValidPort(suffix.substr(1)));
    }
    const auto colon = authority.find(':');
    return ValidHost(authority.substr(0, colon)) &&
           (colon == std::string_view::npos || ValidPort(authority.substr(colon + 1)));
}

bool IsDigest(std::string_view value) {
    return value.size() == 64 && std::ranges::all_of(value, [](char ch) {
               return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
           });
}

bool ValidHeaderValue(std::string_view value) {
    return std::ranges::all_of(value, [](unsigned char ch) { return ch == '\t' || (ch >= 32 && ch != 127); });
}

drogon::HttpMethod ControlMethod(std::string_view method) {
    constexpr std::array methods = {
        std::pair{"GET", drogon::Get},       std::pair{"POST", drogon::Post},
        std::pair{"HEAD", drogon::Head},     std::pair{"PUT", drogon::Put},
        std::pair{"DELETE", drogon::Delete}, std::pair{"OPTIONS", drogon::Options},
        std::pair{"PATCH", drogon::Patch},   std::pair{"PROPFIND", drogon::Propfind},
        std::pair{"MKCOL", drogon::Mkcol},   std::pair{"COPY", drogon::Copy},
        std::pair{"MOVE", drogon::Move},
    };
    for (const auto& [name, value] : methods) {
        if (method == name) {
            return value;
        }
    }
    Missing();
}

drogon::HttpRequestPtr MakeRequest(drogon::HttpMethod method, const std::string& path, std::string body) {
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(method);
    request->setPath(path);
    request->setPassThrough(true);
    if (!body.empty() || method == drogon::Post || method == drogon::Put || method == drogon::Patch) {
        request->addHeader("content-length", std::to_string(body.size()));
    }
    request->setBody(std::move(body));
    return request;
}

void AddControlHeaders(const Request& request, const drogon::HttpRequestPtr& outgoing) {
    for (const auto* name : {"content-type", "idempotency-key", "sec-fetch-site"}) {
        const auto header = std::ranges::find_if(
            request.headers, [name](const auto& item) { return http::EqualIgnoreCase(item.first, name); });
        if (header != request.headers.end()) {
            if (!ValidHeaderValue(header->second)) {
                Bad("The request contains an invalid header.");
            }
            outgoing->addHeader(name, header->second);
        }
    }
    if (IsDigest(request.session)) {
        outgoing->addCookie(kSession, request.session);
    }
    if (ValidHeaderValue(request.peer) && (IsIp(request.peer, AF_INET) || IsIp(request.peer, AF_INET6))) {
        outgoing->addHeader("X-Flipkiln-Client-Peer", request.peer);
    }
}

Response ReadControlResponse(const drogon::HttpResponsePtr& response) {
    if (response->statusCode() == drogon::k403Forbidden) {
        const auto document = Json::parse(response->body(), nullptr, false);
        const auto error = document.find("error");
        if (error != document.end()) {
            const auto code = error->find("code");
            if (code != error->end() && *code == "storage_forbidden") {
                throw StorageUnavailable();
            }
        }
    }
    Response result{static_cast<int>(response->statusCode()), std::string(response->body())};
    const auto& headers = response->headers();
    constexpr std::array allowed = {
        std::pair{"content-type", "Content-Type"},
        std::pair{"cache-control", "Cache-Control"},
    };
    for (const auto& [key, name] : allowed) {
        const auto header = headers.find(key);
        if (header != headers.end()) {
            if (!ValidHeaderValue(header->second)) {
                throw StorageUnavailable();
            }
            result.headers.emplace(name, header->second);
        }
    }
    const auto& cookies = response->cookies();
    const auto cookie = cookies.find(kSession);
    if (cookie != cookies.end()) {
        const auto line = cookie->second.cookieString();
        constexpr std::string_view prefix = "Set-Cookie: ";
        const auto value = std::string_view(line).substr(prefix.size(), line.size() - prefix.size() - 2);
        if (!ValidHeaderValue(value)) {
            throw StorageUnavailable();
        }
        result.headers.emplace("Set-Cookie", value);
    }
    return result;
}
}

class StorageClient::Transport {
public:
    Transport(std::string url, std::string key) : url_(std::move(url)), key_(std::move(key)) { loop_.run(); }

    drogon::HttpResponsePtr Send(const drogon::HttpRequestPtr& request, double timeout = kTimeout) {
        request->addHeader("Host", std::string(Authority(url_)));
        request->addHeader("X-Flipkiln-Storage-Key", key_);
        const auto worker = std::this_thread::get_id();
        drogon::HttpClientPtr client;
        {
            const std::lock_guard lock(mutex_);
            auto& entry = clients_[worker];
            if (!entry) {
                entry = drogon::HttpClient::newHttpClient(url_, loop_.getLoop());
                entry->enableCookies(false);
                entry->setPipeliningDepth(0);
            }
            client = entry;
        }

        const auto [result, response] = client->sendRequest(request, timeout);
        if (result != drogon::ReqResult::Ok || !response) {
            if (result != drogon::ReqResult::Timeout) {
                const std::lock_guard lock(mutex_);
                clients_.erase(worker);
            }
            throw StorageUnavailable();
        }
        return response;
    }

    void Check(const std::string& path) {
        if (Send(MakeRequest(drogon::Get, path, {}), kReadinessTimeout)->statusCode() != drogon::k200OK) {
            throw StorageUnavailable();
        }
    }

private:
    const std::string url_;
    const std::string key_;
    trantor::EventLoopThread loop_{"storage-http"};
    std::mutex mutex_;
    std::map<std::thread::id, drogon::HttpClientPtr> clients_;
};

StorageUnavailable::StorageUnavailable() : std::runtime_error("Storage is unavailable. Try again shortly.") {}

StorageClient::StorageClient(std::string base_url, std::string key) {
    if (!ValidOrigin(base_url)) {
        throw std::invalid_argument("FLIPKILN_STORAGE_URL must be an HTTP or HTTPS origin");
    }
    if (key.size() < 32 || key.size() > 256 ||
        !std::ranges::all_of(key, [](unsigned char ch) { return ch >= 33 && ch <= 126; })) {
        throw std::invalid_argument("FLIPKILN_STORAGE_KEY must contain 32 to 256 visible ASCII characters");
    }
    transport_ = std::make_unique<Transport>(std::move(base_url), std::move(key));
}

StorageClient::~StorageClient() = default;

Response StorageClient::Control(const Request& request) const {
    if ((!request.path.starts_with("/api/auth/") && !request.path.starts_with("/api/manage/")) ||
        request.path.find('?') != std::string::npos) {
        Missing();
    }
    const auto outgoing =
        MakeRequest(ControlMethod(request.method), "/internal/v1/control" + request.path, request.body);
    AddControlHeaders(request, outgoing);
    return ReadControlResponse(transport_->Send(outgoing));
}

std::optional<Json> StorageClient::Snapshot(std::string_view project, std::string_view environment,
                                            std::string_view token_digest) const {
    if (!IsDigest(token_digest)) {
        throw StorageUnavailable();
    }
    std::string body;
    try {
        body = Json{{"project", project}, {"environment", environment}, {"tokenDigest", token_digest}}.dump();
    } catch (const Json::exception&) {
        throw StorageUnavailable();
    }
    const auto outgoing = MakeRequest(drogon::Post, "/internal/v1/runtime/snapshot", std::move(body));
    outgoing->addHeader("Content-Type", "application/json");
    const auto response = transport_->Send(outgoing);
    if (response->statusCode() != drogon::k200OK) {
        throw StorageUnavailable();
    }
    auto document = Json::parse(response->body(), nullptr, false);
    const auto snapshot = document.find("snapshot");
    if (snapshot == document.end()) {
        throw StorageUnavailable();
    }
    if (snapshot->is_null()) {
        return std::nullopt;
    }
    const auto revision = snapshot->find("revision");
    const auto entries = snapshot->find("entries");
    if (revision == snapshot->end() || !revision->is_number_integer() || *revision <= 0 ||
        *revision > std::numeric_limits<int>::max() || entries == snapshot->end() || !entries->is_array()) {
        throw StorageUnavailable();
    }
    return std::move(*snapshot);
}

void StorageClient::CheckControl() const { transport_->Check("/internal/v1/control/ready"); }

void StorageClient::CheckRuntime() const { transport_->Check("/internal/v1/runtime/ready"); }
}
