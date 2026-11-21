#include "migrations.hpp"

#include <libpq-fe.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

#include "config.hpp"
#include "db.hpp"
#include "domain.hpp"
#include "security.hpp"

namespace flipkiln {
namespace {
using Result = std::unique_ptr<PGresult, decltype(&PQclear)>;
using Client = std::unique_ptr<PGconn, decltype(&PQfinish)>;

Result Query(PGconn* connection, std::string_view query, const Params& parameters = {}) {
    const std::string sql(query);
    std::vector<const char*> arguments;
    for (const auto& parameter : parameters) {
        arguments.push_back(parameter.c_str());
    }
    Result result(parameters.empty()
                      ? PQexec(connection, sql.c_str())
                      : PQexecParams(connection, sql.c_str(), static_cast<int>(arguments.size()), nullptr,
                                     arguments.data(), nullptr, nullptr, 0),
                  PQclear);
    if (!result || (PQresultStatus(result.get()) != PGRES_COMMAND_OK &&
                    PQresultStatus(result.get()) != PGRES_TUPLES_OK)) {
        const char* state = result ? PQresultErrorField(result.get(), PG_DIAG_SQLSTATE) : nullptr;
        throw DatabaseError("Migration statement failed (SQLSTATE " +
                            std::string(state ? state : "unavailable") + ")");
    }
    return result;
}

bool HasRow(const Result& result) { return PQntuples(result.get()) != 0; }

std::string File(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to open " + path.string());
    }
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

void SeedProject(PGconn* connection, bool demo, const std::filesystem::path& fixtures) {
    auto created = Query(connection,
                         "INSERT INTO projects(id,name) VALUES ('workshop','Workshop') "
                         "ON CONFLICT DO NOTHING RETURNING id");
    if (!HasRow(created)) {
        return;
    }
    auto entries = Json::array();
    if (demo) {
        entries = Json::parse(File(fixtures)).at("snapshot").at("entries");
        if (!ValidateEntries(entries).empty()) {
            throw std::runtime_error("Workshop fixture failed validation");
        }
    }
    for (const std::string env : {"development", "production"}) {
        const auto seed = env == "development" ? entries : Json::array();
        Query(connection,
              "INSERT INTO environments(project,name,draft_entries,active_revision) "
              "VALUES ('workshop',$1,$2::jsonb,1)",
              {env, seed.dump()});
        Query(connection,
              "INSERT INTO releases(project,environment,revision,kind,entries,actor,note) "
              "VALUES ('workshop',$1,1,'initial',$2::jsonb,'admin','Initial configuration')",
              {env, seed.dump()});
        Query(connection,
              "INSERT INTO entry_types(project,environment,key,type) "
              "SELECT 'workshop',$1,entry->>'key',entry->>'type' FROM jsonb_array_elements($2::jsonb) AS "
              "t(entry)",
              {env, seed.dump()});
    }
}

void GrantRuntimeAccess(PGconn* connection) {
    const auto name = Env("FLIPKILN_RUNTIME_ROLE");
    if (name.empty()) {
        return;
    }
    if (!HasRow(Query(connection,
                      "SELECT 1 FROM pg_roles WHERE rolname=$1 AND rolcanlogin "
                      "AND NOT rolsuper AND rolname<>current_user",
                      {name}))) {
        throw std::runtime_error("FLIPKILN_RUNTIME_ROLE must name an existing, separate PostgreSQL login");
    }
    const std::unique_ptr<char, decltype(&PQfreemem)> quoted(
        PQescapeIdentifier(connection, name.data(), name.size()), PQfreemem);
    if (!quoted) {
        throw std::runtime_error("Could not quote runtime PostgreSQL role");
    }
    Query(connection, "GRANT EXECUTE ON FUNCTION public.flipkiln_client_snapshot(text,text,text) TO " +
                          std::string(quoted.get()));
}

void Seed(PGconn* connection, const std::filesystem::path& fixtures) {
    const bool demo = Env("FLIPKILN_DEMO") == "true";
    const auto password = Env("FLIPKILN_ADMIN_PASSWORD");
    if (!HasRow(Query(connection, "SELECT username FROM users WHERE username='admin'"))) {
        if (password.size() < 12 || password.size() > 256) {
            throw std::runtime_error("Set FLIPKILN_ADMIN_PASSWORD to 12–256 bytes before installing");
        }
        Query(connection, "INSERT INTO users(username,password_hash,global_admin) VALUES ('admin',$1,true)",
              {HashPassword(password)});
    }
    SeedProject(connection, demo, fixtures);
    if (!demo) {
        return;
    }

    const auto demo_password = Env("FLIPKILN_DEMO_PASSWORD", "flipkiln-local-demo");
    if (demo_password.size() < 12 || demo_password.size() > 256) {
        throw std::runtime_error("Set FLIPKILN_DEMO_PASSWORD to 12–256 bytes");
    }
    int role = 0;
    for (const std::string user : {"viewer", "editor", "publisher"}) {
        auto created = HasRow(Query(connection, "SELECT username FROM users WHERE username=$1", {user}));
        if (!created) {
            Query(connection, "INSERT INTO users(username,password_hash) VALUES ($1,$2)",
                  {user, HashPassword(demo_password)});
            Query(connection,
                  "INSERT INTO memberships(project,username,role) VALUES ('workshop',$1,$2::smallint)",
                  {user, std::to_string(role)});
        }
        ++role;
    }
    const auto token = Env("FLIPKILN_DEMO_TOKEN");
    if (token.empty()) {
        return;
    }
    if (token.size() < 20 || token.size() > 256) {
        throw std::runtime_error("Set FLIPKILN_DEMO_TOKEN to 20–256 bytes");
    }
    Query(connection,
          "INSERT INTO tokens(id,project,environment,name,token_hash) "
          "VALUES ('workshop-demo','workshop','development','Workshop demo',$1) ON CONFLICT (id) DO NOTHING",
          {Digest(token)});
}
}

namespace {
Client Connect() {
    const auto dsn = Env("DATABASE_URL");
    if (dsn.empty()) {
        throw std::runtime_error("DATABASE_URL is required");
    }
    const char* keys[] = {"dbname", "connect_timeout", nullptr};
    const char* values[] = {dsn.c_str(), "5", nullptr};
    Client connection(PQconnectdbParams(keys, values, 1), PQfinish);
    if (!connection || PQstatus(connection.get()) != CONNECTION_OK) {
        throw DatabaseError("PostgreSQL is unavailable");
    }
    return connection;
}
}

void Migrate() {
    const auto connection = Connect();
    Query(connection.get(), "BEGIN");
    Query(connection.get(), "SET LOCAL search_path = public, pg_catalog");
    Query(connection.get(), "SET LOCAL lock_timeout = '15s'");
    Query(connection.get(), "SET LOCAL statement_timeout = '120s'");
    Query(connection.get(), "SELECT pg_advisory_xact_lock(582143774380551613)");
    Query(connection.get(),
          "CREATE TABLE IF NOT EXISTS flipkiln_migrations ("
          "version text PRIMARY KEY, checksum text NOT NULL, applied_at timestamptz NOT NULL DEFAULT now())");
    const std::filesystem::path directory =
        Env("FLIPKILN_MIGRATIONS", "/usr/local/share/flipkiln/migrations");
    std::vector<std::filesystem::path> files;
    for (const auto& file : std::filesystem::directory_iterator(directory)) {
        if (file.is_regular_file() &&
            std::regex_match(file.path().filename().string(), std::regex(R"(^[0-9]{3}_.+\.sql$)"))) {
            files.push_back(file.path());
        }
    }
    std::sort(files.begin(), files.end());
    if (files.empty()) {
        throw std::runtime_error("No migration files found");
    }
    for (const auto& path : files) {
        const auto version = path.filename().string();
        const auto sql = File(path);
        const auto checksum = Digest(sql);
        const auto previous =
            Query(connection.get(), "SELECT checksum FROM flipkiln_migrations WHERE version=$1", {version});
        if (HasRow(previous)) {
            if (checksum != PQgetvalue(previous.get(), 0, 0)) {
                throw std::runtime_error("Applied migration was edited: " + version);
            }
            continue;
        }
        try {
            Query(connection.get(), sql);
        } catch (const DatabaseError& error) {
            throw DatabaseError(version + ": " + error.what());
        }
        Query(connection.get(), "INSERT INTO flipkiln_migrations(version,checksum) VALUES ($1,$2)",
              {version, checksum});
    }
    const std::string mode = Env("FLIPKILN_DEMO") == "true" ? "demo" : "standard";
    if (HasRow(Query(connection.get(), "SELECT mode FROM flipkiln_installation WHERE mode<>$1", {mode}))) {
        throw std::runtime_error(
            "Installation mode is fixed. Use a separate PostgreSQL database for demo mode.");
    }
    Query(connection.get(),
          "INSERT INTO flipkiln_installation(singleton,mode) VALUES (true,$1) ON CONFLICT DO NOTHING",
          {mode});
    GrantRuntimeAccess(connection.get());
    Seed(connection.get(), Env("FLIPKILN_FIXTURE", "/usr/local/share/flipkiln/workshop.json"));
    Query(connection.get(), "COMMIT");
    std::cerr << "Database migrations complete\n";
}

void CreateUser(std::string_view username, std::string_view password) {
    if (!ValidIdentifier(username)) {
        throw std::runtime_error("Username must be a lowercase identifier of at most 64 bytes");
    }
    if (password.size() < 12 || password.size() > 256 || password.find('\0') != std::string_view::npos) {
        throw std::runtime_error("Supply a password of 12–256 bytes on standard input");
    }
    const auto connection = Connect();
    if (!HasRow(Query(connection.get(),
                      "INSERT INTO users(username,password_hash) VALUES ($1,$2) "
                      "ON CONFLICT DO NOTHING RETURNING username",
                      {std::string(username), HashPassword(password)}))) {
        throw std::runtime_error("Username already exists");
    }
    std::cerr << "Created user " << username << '\n';
}
}
