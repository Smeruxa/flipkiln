#include "routes.hpp"

#include <algorithm>
#include <charconv>

#include "storage/access.hpp"
#include "storage/releases.hpp"

namespace flipkiln::control {
namespace {
using storage::Actor;
using storage::Role;
using storage::Scope;

const std::string& ProjectId(const std::vector<std::string>& path) {
    if (path.size() < 5 || !http::ValidProject(path.at(3))) {
        Missing();
    }
    return path.at(3);
}

Scope EnvironmentScope(const std::vector<std::string>& path, const std::string& project) {
    if (path.size() < 7 || path.at(4) != "environments") {
        Missing();
    }
    const auto& environment = path.at(5);
    if (environment != "development" && environment != "production") {
        Missing();
    }
    return {project, environment};
}

Response CreateProject(Database& db, const Request& request, const Actor& actor) {
    if (request.method != "POST") {
        Missing();
    }
    if (!actor.global_admin) {
        Forbidden();
    }
    const auto body = http::Body(request);
    const auto id = http::ReadId(http::Field(body, "id"));
    if (!http::ValidProject(id)) {
        Bad("Use up to 63 lowercase letters, digits, or hyphens in a project identifier.");
    }
    const auto name = http::ReadString(http::Field(body, "name"), 1, 100);
    return http::Send(201, storage::CreateProject(db, actor, id, name));
}

Response ReplaceMember(Database& db, const Request& request, const Actor& actor, const std::string& project,
                       const std::vector<std::string>& path) {
    if (path.size() != 6 || request.method != "PUT") {
        Missing();
    }
    const auto username = http::ReadId(path.at(5));
    const auto body = http::Body(request);
    const auto& role = http::Field(body, "role");
    std::optional<Role> parsed;
    if (!role.is_null()) {
        parsed = storage::ParseRole(http::ReadString(role, 1, 20));
        if (!parsed) {
            Bad("Unknown project role.");
        }
    }
    storage::ReplaceMember(db, actor, project, username, parsed);
    return http::Send(200, {{"username", username}, {"role", role}});
}

Response ChangeToken(Database& db, const Request& request, const Actor& actor, const Scope& scope,
                     const std::vector<std::string>& path) {
    if (path.size() == 7 && request.method == "POST") {
        const auto name = http::ReadString(http::Field(http::Body(request), "name"), 1, 100);
        return http::Send(201, storage::CreateToken(db, actor, scope, name));
    }
    if (path.size() == 8 && request.method == "DELETE") {
        http::RequireSameSite(request);
        storage::RevokeToken(db, actor, scope, path.at(7));
        return {204, "", {{"Cache-Control", "no-store"}}};
    }
    Missing();
}

storage::ReleaseRequest ReleaseBody(const Request& request, bool rollback) {
    const auto body = http::Body(request);
    const auto version = http::ReadInt(http::Field(body, "draftVersion"));
    std::optional<int> target;
    if (rollback) {
        target = http::ReadInt(http::Field(body, "revision"));
    }
    const auto note = body.contains("note") ? http::ReadString(body.at("note"), 0, 1024) : "";
    const auto key = http::Header(request, "idempotency-key");
    if (key.empty() || key.size() > 128 ||
        !std::ranges::all_of(key, [](unsigned char c) { return c >= 33 && c <= 126; })) {
        Bad("Provide a nonempty Idempotency-Key header of at most 128 visible ASCII characters.");
    }
    return {version, target, note, std::string(key)};
}

int Revision(std::string_view value) {
    int revision = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), revision);
    if (error != std::errc{} || end != value.data() + value.size() || revision <= 0) {
        Missing();
    }
    return revision;
}

Response CheckDraft(Database& db, const Request& request, const Actor& actor, const Scope& scope,
                    bool preview) {
    storage::RequireRole(db, actor, scope.project, Role::Editor);
    const auto body = http::Body(request);
    const auto& entries = http::Field(body, "entries");
    auto issues = storage::Validation(db, scope, entries);
    if (!preview) {
        return http::Send(200, {{"valid", issues.empty()}, {"issues", std::move(issues)}});
    }
    RequireValid(std::move(issues));
    return http::Send(200,
                      {{"results", http::EvaluateBody(entries, body, scope.project, scope.environment)}});
}

Response ReadManagement(Connection& connection, const Actor& actor, const std::vector<std::string>& path) {
    if (path.size() == 3) {
        return http::Send(200, {{"projects", storage::Projects(connection, actor)}});
    }
    const auto& project = ProjectId(path);
    if (path.size() == 5 && path.at(4) == "members") {
        return http::Send(200, {{"members", storage::Members(connection, actor, project)}});
    }
    const auto scope = EnvironmentScope(path, project);
    const auto& action = path.at(6);
    if (path.size() == 7 && action == "tokens") {
        return http::Send(200, {{"tokens", storage::Tokens(connection, actor, scope)}});
    }
    if (path.size() == 8 && action == "releases") {
        storage::RequireRole(connection, actor, project, Role::Viewer);
        return http::Send(200, storage::ReleaseDetail(connection, scope, Revision(path.at(7))));
    }
    if (path.size() == 7 && (action == "state" || action == "releases")) {
        storage::RequireRole(connection, actor, project, Role::Viewer);
        if (action == "state") {
            return http::Send(200, storage::State(connection, scope));
        }
        return http::Send(200, {{"releases", storage::History(connection, scope)}});
    }
    Missing();
}

Response ChangeEnvironment(Database& db, const Request& request, const Actor& actor, const Scope& scope,
                           const std::vector<std::string>& path) {
    const auto& action = path.at(6);
    if (action == "tokens") {
        return ChangeToken(db, request, actor, scope, path);
    }
    if (path.size() != 7) {
        Missing();
    }
    if (request.method == "PUT" && action == "draft") {
        const auto body = http::Body(request);
        const auto version = http::ReadInt(http::Field(body, "draftVersion"));
        return http::Send(200, storage::SaveDraft(db, actor, scope, version, http::Field(body, "entries")));
    }
    if (request.method == "POST" && (action == "validate" || action == "preview")) {
        return CheckDraft(db, request, actor, scope, action == "preview");
    }
    if (request.method == "POST" && (action == "publish" || action == "rollback")) {
        return http::Send(200,
                          storage::Publish(db, actor, scope, ReleaseBody(request, action == "rollback")));
    }
    Missing();
}
}

Response ManagementRoute(Database& db, const Request& request, const std::vector<std::string>& path) {
    if (request.method == "GET") {
        auto transaction = db.Begin(TransactionMode::ReadSnapshot);
        const auto actor = storage::Authenticate(*transaction, request.session);
        auto response = ReadManagement(*transaction, actor, path);
        transaction->Commit();
        return response;
    }

    const auto actor = storage::Authenticate(db, request.session);
    if (path.size() == 3) {
        return CreateProject(db, request, actor);
    }
    const auto& project = ProjectId(path);
    if (path.at(4) == "members") {
        return ReplaceMember(db, request, actor, project, path);
    }
    return ChangeEnvironment(db, request, actor, EnvironmentScope(path, project), path);
}
}
