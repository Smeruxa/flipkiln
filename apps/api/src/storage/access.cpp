#include "access.hpp"

#include <algorithm>
#include <array>

#include "json_rows.hpp"
#include "problem.hpp"
#include "security.hpp"

namespace flipkiln::storage {
namespace {
constexpr std::array<std::string_view, 4> kRoles{"viewer", "editor", "publisher", "admin"};
constexpr std::string_view kTokenColumns = R"(
    jsonb_build_object('id', id, 'name', name,
        'createdAt', to_char(created_at AT TIME ZONE 'UTC','YYYY-MM-DD"T"HH24:MI:SS.MS"Z"'),
        'revokedAt', to_char(revoked_at AT TIME ZONE 'UTC','YYYY-MM-DD"T"HH24:MI:SS.MS"Z"'))
)";
}

std::optional<Role> ParseRole(std::string_view role) {
    const auto position = std::ranges::find(kRoles, role);
    if (position == kRoles.end()) {
        return std::nullopt;
    }
    return static_cast<Role>(position - kRoles.begin());
}

void CheckControlSchema(Connection& connection) {
    if (connection.Query("SELECT 1 FROM flipkiln_migrations WHERE version='002_runtime.sql'").empty()) {
        throw DatabaseError("Required schema unavailable");
    }
}

Actor Authenticate(Connection& connection, std::string_view session) {
    if (session.size() != 64) {
        Unauthorized();
    }
    const auto rows = connection.Query(
        "SELECT u.username, u.global_admin::text FROM sessions s JOIN users u USING(username) "
        "WHERE s.token_hash=$1 AND s.expires_at>now()",
        {Digest(session)});
    if (rows.empty()) {
        Unauthorized();
    }
    return {rows.at(0).at(0).value(), rows.at(0).at(1).value() == "true"};
}

void RequireRole(Connection& connection, const Actor& actor, const std::string& project, Role minimum,
                 bool lock) {
    if (actor.global_admin) {
        if (connection.Query("SELECT 1 FROM projects WHERE id=$1", {project}).empty()) {
            Missing();
        }
        return;
    }
    const auto sql = lock ? "SELECT role::text FROM memberships WHERE project=$1 AND username=$2 FOR SHARE"
                          : "SELECT role::text FROM memberships WHERE project=$1 AND username=$2";
    const auto rows = connection.Query(sql, {project, actor.username});
    if (rows.empty()) {
        Missing();
    }
    if (std::stoi(rows.at(0).at(0).value()) < static_cast<int>(minimum)) {
        Forbidden();
    }
}

std::optional<std::string> PasswordHashFor(Connection& connection, const std::string& username) {
    const auto rows = connection.Query("SELECT password_hash FROM users WHERE username=$1", {username});
    return rows.empty() ? std::nullopt : rows.at(0).at(0);
}

std::string StartSession(Database& db, const std::string& username, std::string_view previous) {
    const auto session = RandomToken();
    auto transaction = db.Begin();
    transaction->Query("DELETE FROM sessions WHERE expires_at <= now()");
    EndSession(*transaction, previous);
    transaction->Query("INSERT INTO sessions(token_hash,username) VALUES ($1,$2)",
                       {Digest(session), username});
    transaction->Commit();
    return session;
}

void EndSession(Connection& connection, std::string_view session) {
    if (session.size() == 64) {
        connection.Query("DELETE FROM sessions WHERE token_hash=$1", {Digest(session)});
    }
}

Json Projects(Connection& connection, const Actor& actor) {
    return ReadArray(connection.Query(R"(
        SELECT jsonb_build_object('id',p.id, 'name',p.name,
            'role', (ARRAY['viewer', 'editor', 'publisher', 'admin'])[
                CASE WHEN $2::boolean THEN 4 ELSE m.role + 1 END
            ],
            'environments',jsonb_build_array('development','production'))::text
        FROM projects p LEFT JOIN memberships m ON m.project=p.id AND m.username=$1
        WHERE $2::boolean OR m.username IS NOT NULL ORDER BY p.name,p.id
    )",
                                      {actor.username, actor.global_admin ? "true" : "false"}));
}

Json CreateProject(Database& db, const Actor& actor, const std::string& id, const std::string& name) {
    if (!actor.global_admin) {
        Forbidden();
    }
    auto transaction = db.Begin();
    if (transaction
            ->Query("INSERT INTO projects(id,name) VALUES ($1,$2) ON CONFLICT DO NOTHING RETURNING id",
                    {id, name})
            .empty()) {
        throw Problem{ProblemKind::Conflict, "project_exists",
                      "A project with that identifier already exists."};
    }
    for (const std::string environment : {"development", "production"}) {
        transaction->Query(
            "INSERT INTO environments(project,name,draft_entries,active_revision) VALUES ($1,$2,'[]',1)",
            {id, environment});
        transaction->Query(
            "INSERT INTO releases(project,environment,revision,kind,entries,actor,note) "
            "VALUES ($1,$2,1,'initial','[]',$3,'Initial configuration')",
            {id, environment, actor.username});
    }
    transaction->Commit();
    return {{"id", id}, {"name", name}, {"role", "admin"}, {"environments", {"development", "production"}}};
}

Json Members(Connection& connection, const Actor& actor, const std::string& project) {
    RequireRole(connection, actor, project, Role::Admin);
    return ReadArray(connection.Query(R"(
        SELECT jsonb_build_object('username',u.username,
            'role', (ARRAY['viewer', 'editor', 'publisher', 'admin'])[
                CASE WHEN u.global_admin THEN 4 ELSE m.role + 1 END
            ])::text
        FROM users u LEFT JOIN memberships m ON m.username=u.username AND m.project=$1
        WHERE u.global_admin OR m.username IS NOT NULL ORDER BY u.username
    )",
                                      {project}));
}

void ReplaceMember(Database& db, const Actor& actor, const std::string& project, const std::string& username,
                   std::optional<Role> role) {
    auto transaction = db.Begin();

    if (transaction->Query("SELECT 1 FROM projects WHERE id=$1 FOR UPDATE", {project}).empty()) {
        Missing();
    }
    RequireRole(*transaction, actor, project, Role::Admin, true);
    const auto users =
        transaction->Query("SELECT global_admin::text FROM users WHERE username=$1", {username});
    if (users.empty()) {
        Missing();
    }
    if (users.at(0).at(0).value() == "true") {
        throw Problem{ProblemKind::Conflict, "bootstrap_admin",
                      "The installation administrator has access to every project."};
    }
    if (role) {
        transaction->Query(
            "INSERT INTO memberships(project,username,role) VALUES ($1,$2,$3::smallint) "
            "ON CONFLICT (project,username) DO UPDATE SET role=EXCLUDED.role",
            {project, username, std::to_string(static_cast<int>(*role))});
    } else {
        transaction->Query("DELETE FROM memberships WHERE project=$1 AND username=$2", {project, username});
    }
    transaction->Commit();
}

Json Tokens(Connection& connection, const Actor& actor, const Scope& scope) {
    RequireRole(connection, actor, scope.project, Role::Admin);
    return ReadArray(connection.Query(
        "SELECT " + std::string(kTokenColumns) +
            "::text FROM tokens WHERE project=$1 AND environment=$2 ORDER BY created_at DESC,id",
        scope.Parameters()));
}

Json CreateToken(Database& db, const Actor& actor, const Scope& scope, const std::string& name) {
    auto transaction = db.Begin();
    RequireRole(*transaction, actor, scope.project, Role::Admin, true);
    const auto token = RandomToken();
    auto created = ReadJson(transaction->Query(
        "INSERT INTO tokens(id,project,environment,name,token_hash) VALUES ($3,$1,$2,$4,$5) RETURNING " +
            std::string(kTokenColumns) + "::text",
        {scope.project, scope.environment, RandomToken().substr(0, 24), name, Digest(token)}));
    created["token"] = token;
    transaction->Commit();
    return created;
}

void RevokeToken(Database& db, const Actor& actor, const Scope& scope, const std::string& id) {
    auto transaction = db.Begin();
    RequireRole(*transaction, actor, scope.project, Role::Admin, true);
    const auto rows = transaction->Query(
        "UPDATE tokens SET revoked_at=COALESCE(revoked_at,now()) WHERE project=$1 AND environment=$2 AND "
        "id=$3 RETURNING id",
        {scope.project, scope.environment, id});
    if (rows.empty()) {
        Missing();
    }
    transaction->Commit();
}
}
