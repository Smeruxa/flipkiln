#include "releases.hpp"

#include "json_rows.hpp"

namespace flipkiln::storage {
namespace {
constexpr std::string_view kReleaseColumns = R"(
    jsonb_build_object('revision', revision, 'kind', kind, 'sourceRevision', source_revision,
      'createdAt', to_char(created_at AT TIME ZONE 'UTC','YYYY-MM-DD"T"HH24:MI:SS.MS"Z"'),
      'actor', actor, 'note', note)
)";
}

std::optional<LockedDraft> LockDraft(Transaction& transaction, const Scope& scope) {
    const auto rows = transaction.Query(
        "SELECT draft_version::text, active_revision::text, draft_entries::text "
        "FROM environments WHERE project=$1 AND name=$2 FOR UPDATE",
        ScopeParameters(scope));
    if (rows.empty()) {
        return std::nullopt;
    }
    const auto& row = rows.front();
    return LockedDraft{std::stoi(row.at(0).value()), std::stoi(row.at(1).value()), row.at(2).value()};
}

std::optional<Json> FindState(Connection& connection, const Scope& scope) {
    const auto rows = connection.Query(R"(
        SELECT jsonb_build_object('draftVersion', draft_version, 'entries', draft_entries,
            'active', jsonb_build_object('revision', active_revision, 'entries', entries))::text
        FROM environments e
        JOIN releases r ON r.project = e.project
            AND r.environment = e.name
            AND r.revision = e.active_revision
        WHERE e.project = $1 AND e.name = $2
    )",
                                       ScopeParameters(scope));
    if (rows.empty()) {
        return std::nullopt;
    }
    return ReadJson(rows);
}

std::unordered_map<std::string, std::string> EntryTypes(Connection& connection, const Scope& scope) {
    const auto rows = connection.Query(
        "SELECT key, type FROM entry_types WHERE project=$1 AND environment=$2", ScopeParameters(scope));
    std::unordered_map<std::string, std::string> types;
    for (const auto& row : rows) {
        types.emplace(row.at(0).value(), row.at(1).value());
    }
    return types;
}

void UpdateDraft(Transaction& transaction, const Scope& scope, const Json& entries) {
    transaction.Query(
        "UPDATE environments SET draft_entries=$3::jsonb, draft_version=draft_version+1 "
        "WHERE project=$1 AND name=$2",
        {scope.project, scope.environment, entries.dump()});
}

Json History(Connection& connection, const Scope& scope) {
    const auto query = "SELECT " + std::string(kReleaseColumns) +
                       "::text FROM releases WHERE project=$1 AND environment=$2 ORDER BY revision DESC";
    return ReadArray(connection.Query(query, ScopeParameters(scope)));
}

std::optional<Json> FindReleaseDetail(Connection& connection, const Scope& scope, int revision) {
    const auto query = "SELECT (" + std::string(kReleaseColumns) +
                       " || jsonb_build_object('entries',entries))::text FROM releases "
                       "WHERE project=$1 AND environment=$2 AND revision=$3::int";
    const auto rows = connection.Query(query, {scope.project, scope.environment, std::to_string(revision)});
    if (rows.empty()) {
        return std::nullopt;
    }
    return ReadJson(rows);
}

std::optional<Snapshot> FindSnapshot(Connection& connection, const Scope& scope, int revision) {
    const auto rows = connection.Query(
        "SELECT revision::text, entries::text FROM releases "
        "WHERE project=$1 AND environment=$2 AND revision=$3::int",
        {scope.project, scope.environment, std::to_string(revision)});
    if (rows.empty()) {
        return std::nullopt;
    }
    return Snapshot{std::stoi(rows.front().at(0).value()), ReadJson(rows, 1)};
}

std::optional<ReleaseOperation> FindOperation(Connection& connection, const Scope& scope,
                                              const std::string& actor, const std::string& key) {
    const auto rows = connection.Query(
        "SELECT parameters::text, revision::text FROM operation_keys "
        "WHERE project=$1 AND environment=$2 AND actor=$3 AND key=$4",
        {scope.project, scope.environment, actor, key});
    if (rows.empty()) {
        return std::nullopt;
    }
    return ReleaseOperation{ReadJson(rows), std::stoi(rows.front().at(1).value())};
}

void InsertRelease(Transaction& transaction, const Scope& scope, const ReleaseRecord& release,
                   const std::string& entries) {
    const auto source = release.source_revision ? std::to_string(*release.source_revision) : "";
    transaction.Query(
        "INSERT INTO releases(project,environment,revision,kind,source_revision,entries,actor,note) "
        "VALUES ($1,$2,$3::int,$4,NULLIF($5,'')::int,$6::jsonb,$7,$8)",
        {scope.project, scope.environment, std::to_string(release.revision), std::string(release.kind),
         source, entries, std::string(release.actor), std::string(release.note)});
}

void ActivateRelease(Transaction& transaction, const Scope& scope, int revision, const std::string& entries) {
    transaction.Query(
        "UPDATE environments SET "
        "draft_entries=$3::jsonb, active_revision=$4::int, draft_version=draft_version+1 "
        "WHERE project=$1 AND name=$2",
        {scope.project, scope.environment, entries, std::to_string(revision)});
}

void RecordEntryTypes(Transaction& transaction, const Scope& scope, const std::string& entries) {
    transaction.Query(
        "INSERT INTO entry_types(project,environment,key,type) "
        "SELECT $1,$2,entry->>'key',entry->>'type' FROM jsonb_array_elements($3::jsonb) AS t(entry) "
        "ON CONFLICT DO NOTHING",
        {scope.project, scope.environment, entries});
}

void RecordOperation(Transaction& transaction, const Scope& scope, const std::string& actor,
                     const std::string& key, const ReleaseOperation& operation) {
    transaction.Query(
        "INSERT INTO operation_keys(project,environment,actor,key,parameters,revision) "
        "VALUES ($1,$2,$3,$4,$5::jsonb,$6::int)",
        {scope.project, scope.environment, actor, key, operation.parameters.dump(),
         std::to_string(operation.revision)});
}
}
