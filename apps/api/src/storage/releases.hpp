#pragma once

#include <unordered_map>

#include "domain.hpp"
#include "scope.hpp"

namespace flipkiln::storage {
struct LockedDraft {
    int version;
    int active_revision;
    std::string serialized_entries;
};

struct ReleaseOperation {
    Json parameters;
    int revision;
};

struct ReleaseRecord {
    int revision;
    std::optional<int> source_revision;
    std::string_view kind;
    std::string_view actor;
    std::string_view note;
};

std::optional<LockedDraft> LockDraft(Transaction& transaction, const Scope& scope);
std::optional<Json> FindState(Connection& connection, const Scope& scope);
std::unordered_map<std::string, std::string> EntryTypes(Connection& connection, const Scope& scope);
void UpdateDraft(Transaction& transaction, const Scope& scope, const Json& entries);
Json History(Connection& connection, const Scope& scope);
std::optional<Json> FindReleaseDetail(Connection& connection, const Scope& scope, int revision);
std::optional<Snapshot> FindSnapshot(Connection& connection, const Scope& scope, int revision);
std::optional<ReleaseOperation> FindOperation(Connection& connection, const Scope& scope,
                                              const std::string& actor, const std::string& key);
void InsertRelease(Transaction& transaction, const Scope& scope, const ReleaseRecord& release,
                   const std::string& entries);
void ActivateRelease(Transaction& transaction, const Scope& scope, int revision, const std::string& entries);
void RecordEntryTypes(Transaction& transaction, const Scope& scope, const std::string& entries);
void RecordOperation(Transaction& transaction, const Scope& scope, const std::string& actor,
                     const std::string& key, const ReleaseOperation& operation);
}
