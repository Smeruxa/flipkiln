#include "releases.hpp"

#include <limits>
#include <unordered_map>

#include "json_rows.hpp"
#include "problem.hpp"

namespace flipkiln::storage {
namespace {
constexpr int kMaxVersion = std::numeric_limits<std::int32_t>::max();
constexpr std::string_view kReleaseColumns = R"(
    jsonb_build_object('revision', revision, 'kind', kind, 'sourceRevision', source_revision,
      'createdAt', to_char(created_at AT TIME ZONE 'UTC','YYYY-MM-DD"T"HH24:MI:SS.MS"Z"'),
      'actor', actor, 'note', note)
)";

struct LockedDraft {
    int version;
    int active_revision;
    std::string serialized_entries;
};

LockedDraft LockEnvironment(Connection& connection, const Scope& scope) {
    const auto rows = connection.Query(
        "SELECT draft_version::text,active_revision::text,draft_entries::text "
        "FROM environments WHERE project=$1 AND name=$2 FOR UPDATE",
        scope.Parameters());
    if (rows.empty()) {
        Missing();
    }
    const auto& row = rows.front();
    return {std::stoi(row.at(0).value()), std::stoi(row.at(1).value()), row.at(2).value()};
}

Json HistoricalSnapshot(Connection& db, const Scope& scope, const std::string& revision) {
    const auto rows = db.Query(
        "SELECT revision::text,entries::text FROM releases "
        "WHERE project=$1 AND environment=$2 AND revision=$3::int",
        {scope.project, scope.environment, revision});
    if (rows.empty()) {
        Missing();
    }
    return {{"revision", std::stoi(rows.at(0).at(0).value())}, {"entries", ReadJson(rows, 1)}};
}

}

Json State(Connection& db, const Scope& scope) {
    const auto rows = db.Query(R"(
      SELECT jsonb_build_object('draftVersion',draft_version,'entries',draft_entries,
        'active',jsonb_build_object('revision',active_revision,'entries',entries))::text
      FROM environments e
      JOIN releases r ON r.project = e.project
        AND r.environment = e.name
        AND r.revision = e.active_revision
      WHERE e.project = $1 AND e.name = $2
    )",
                               scope.Parameters());
    if (rows.empty()) {
        Missing();
    }
    return ReadJson(rows);
}

Json Validation(Connection& db, const Scope& scope, const Json& entries) {
    auto issues = ValidateEntries(entries);
    if (!issues.empty()) {
        return issues;
    }
    const auto registered =
        db.Query("SELECT key, type FROM entry_types WHERE project=$1 AND environment=$2", scope.Parameters());
    std::unordered_map<std::string, std::string> types;
    for (const auto& row : registered) {
        types.emplace(row.at(0).value(), row.at(1).value());
    }
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto& entry = entries.at(index);
        const auto found = types.find(entry.at("key").get<std::string>());
        if (found != types.end() && entry.at("type") != found->second) {
            issues.push_back(
                {{"path", "/entries/" + std::to_string(index) + "/type"},
                 {"message", "This key has a published type. Choose a new key to change type."}});
        }
    }
    return issues;
}

Json SaveDraft(Database& db, const Actor& actor, const Scope& scope, int version, const Json& entries) {
    auto tx = db.Begin();
    RequireRole(*tx, actor, scope.project, Role::Editor, true);
    const auto locked = LockEnvironment(*tx, scope);
    if (locked.version != version) {
        DraftConflict();
    }
    if (version == kMaxVersion) {
        throw Problem{ProblemKind::Conflict, "version_exhausted",
                      "This environment reached its maximum draft version."};
    }
    RequireValid(Validation(*tx, scope, entries));
    tx->Query(
        "UPDATE environments SET draft_entries=$3::jsonb,draft_version=draft_version+1 "
        "WHERE project=$1 AND name=$2",
        {scope.project, scope.environment, entries.dump()});
    const auto state = State(*tx, scope);
    tx->Commit();
    return state;
}

Json History(Connection& connection, const Scope& scope) {
    return ReadArray(connection.Query(
        "SELECT " + std::string(kReleaseColumns) +
            "::text FROM releases WHERE project=$1 AND environment=$2 ORDER BY revision DESC",
        scope.Parameters()));
}

Json ReleaseDetail(Connection& connection, const Scope& scope, int revision) {
    const auto rows = connection.Query("SELECT (" + std::string(kReleaseColumns) +
                                           " || jsonb_build_object('entries',entries))::text FROM releases "
                                           "WHERE project=$1 AND environment=$2 AND revision=$3::int",
                                       {scope.project, scope.environment, std::to_string(revision)});
    if (rows.empty()) {
        Missing();
    }
    return ReadJson(rows);
}

Json Publish(Database& db, const Actor& actor, const Scope& scope, const ReleaseRequest& request) {
    const auto version = request.draft_version;
    const auto target = request.rollback_revision ? std::to_string(*request.rollback_revision) : "";
    const std::string action = request.rollback_revision ? "rollback" : "publish";
    const auto& note = request.note;
    const Json parameters{
        {"action", action}, {"draftVersion", version}, {"revision", target}, {"note", note}};

    auto tx = db.Begin();
    RequireRole(*tx, actor, scope.project, Role::Publisher, true);
    const auto locked = LockEnvironment(*tx, scope);
    auto replay = tx->Query(
        "SELECT parameters::text,revision::text FROM operation_keys "
        "WHERE project=$1 AND environment=$2 AND actor=$3 AND key=$4",
        {scope.project, scope.environment, actor.username, request.idempotency_key});
    if (!replay.empty()) {
        if (ReadJson(replay) != parameters) {
            throw Problem{ProblemKind::Conflict, "idempotency_conflict",
                          "This request key was already used with different parameters."};
        }
        const auto snapshot = HistoricalSnapshot(*tx, scope, replay.at(0).at(1).value());
        tx->Commit();
        return snapshot;
    }
    if (locked.version != version) {
        DraftConflict();
    }
    if (version == kMaxVersion) {
        throw Problem{ProblemKind::Conflict, "version_exhausted",
                      "This environment reached its maximum draft version."};
    }
    const auto entries = target.empty() ? Json::parse(locked.serialized_entries)
                                        : HistoricalSnapshot(*tx, scope, target).at("entries");
    RequireValid(Validation(*tx, scope, entries));
    const auto active_revision = locked.active_revision;
    if (active_revision == kMaxVersion) {
        throw Problem{ProblemKind::Conflict, "version_exhausted",
                      "This environment reached its maximum release revision."};
    }
    const auto revision = active_revision + 1;
    const auto number = std::to_string(revision);
    const auto serialized = entries.dump();
    tx->Query(
        "INSERT INTO releases(project,environment,revision,kind,source_revision,entries,actor,note) "
        "VALUES ($1,$2,$3::int,$4,NULLIF($5,'')::int,$6::jsonb,$7,$8)",
        {scope.project, scope.environment, number, action, target, serialized, actor.username, note});
    tx->Query(
        "UPDATE environments SET "
        "draft_entries=$3::jsonb,active_revision=$4::int,draft_version=draft_version+1 "
        "WHERE project=$1 AND name=$2",
        {scope.project, scope.environment, serialized, number});
    tx->Query(
        "INSERT INTO entry_types(project,environment,key,type) "
        "SELECT $1,$2,entry->>'key',entry->>'type' FROM jsonb_array_elements($3::jsonb) AS t(entry) "
        "ON CONFLICT DO NOTHING",
        {scope.project, scope.environment, serialized});
    tx->Query(
        "INSERT INTO operation_keys(project,environment,actor,key,parameters,revision) "
        "VALUES ($1,$2,$3,$4,$5::jsonb,$6::int)",
        {scope.project, scope.environment, actor.username, request.idempotency_key, parameters.dump(),
         number});
    tx->Commit();
    return {{"revision", revision}, {"entries", entries}};
}

}
