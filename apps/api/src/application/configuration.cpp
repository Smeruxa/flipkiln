#include "configuration.hpp"

#include <limits>

#include "problem.hpp"
#include "storage/releases.hpp"

namespace flipkiln::application {
namespace {
constexpr int kMaxVersion = std::numeric_limits<std::int32_t>::max();

Json ReadEnvironmentState(Connection& connection, const Scope& scope) {
    auto state = storage::FindState(connection, scope);
    if (!state) {
        Missing();
    }
    return std::move(*state);
}

storage::LockedDraft LockDraft(Transaction& transaction, const Scope& scope) {
    auto draft = storage::LockDraft(transaction, scope);
    if (!draft) {
        Missing();
    }
    return std::move(*draft);
}

Snapshot ReadHistoricalSnapshot(Connection& connection, const Scope& scope, int revision) {
    auto snapshot = storage::FindSnapshot(connection, scope, revision);
    if (!snapshot) {
        Missing();
    }
    return std::move(*snapshot);
}

void RequireWritableVersion(const storage::LockedDraft& draft, int expected_version) {
    if (draft.version != expected_version) {
        DraftConflict();
    }
    if (expected_version == kMaxVersion) {
        throw Problem{ProblemKind::Conflict, "version_exhausted",
                      "This environment reached its maximum draft version."};
    }
}
}

Json EnvironmentState(Connection& connection, const storage::Actor& actor, const Scope& scope) {
    storage::RequireRole(connection, actor, scope.project, storage::Role::Viewer);
    return ReadEnvironmentState(connection, scope);
}

Json ReleaseHistory(Connection& connection, const storage::Actor& actor, const Scope& scope) {
    storage::RequireRole(connection, actor, scope.project, storage::Role::Viewer);
    return storage::History(connection, scope);
}

Json ReleaseDetail(Connection& connection, const storage::Actor& actor, const Scope& scope, int revision) {
    storage::RequireRole(connection, actor, scope.project, storage::Role::Viewer);
    auto release = storage::FindReleaseDetail(connection, scope, revision);
    if (!release) {
        Missing();
    }
    return std::move(*release);
}

Json ValidateDraft(Connection& connection, const Scope& scope, const Json& entries) {
    auto issues = ValidateEntries(entries);
    if (!issues.empty()) {
        return issues;
    }
    const auto types = storage::EntryTypes(connection, scope);
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

Json SaveDraft(Database& db, const storage::Actor& actor, const Scope& scope, int version,
               const Json& entries) {
    auto transaction = db.Begin();
    storage::RequireRole(*transaction, actor, scope.project, storage::Role::Editor, true);
    const auto draft = LockDraft(*transaction, scope);
    RequireWritableVersion(draft, version);
    RequireValid(ValidateDraft(*transaction, scope, entries));
    storage::UpdateDraft(*transaction, scope, entries);
    auto state = ReadEnvironmentState(*transaction, scope);
    transaction->Commit();
    return state;
}

Snapshot Publish(Database& db, const storage::Actor& actor, const Scope& scope,
                 const ReleaseRequest& request) {
    const auto target = request.rollback_revision ? std::to_string(*request.rollback_revision) : "";
    const std::string action = request.rollback_revision ? "rollback" : "publish";
    const Json parameters{{"action", action},
                          {"draftVersion", request.draft_version},
                          {"revision", target},
                          {"note", request.note}};

    auto transaction = db.Begin();
    storage::RequireRole(*transaction, actor, scope.project, storage::Role::Publisher, true);
    const auto draft = LockDraft(*transaction, scope);
    const auto previous =
        storage::FindOperation(*transaction, scope, actor.username, request.idempotency_key);
    if (previous) {
        if (previous->parameters != parameters) {
            throw Problem{ProblemKind::Conflict, "idempotency_conflict",
                          "This request key was already used with different parameters."};
        }
        auto snapshot = ReadHistoricalSnapshot(*transaction, scope, previous->revision);
        transaction->Commit();
        return snapshot;
    }

    RequireWritableVersion(draft, request.draft_version);
    auto entries = request.rollback_revision
                       ? ReadHistoricalSnapshot(*transaction, scope, *request.rollback_revision).entries
                       : Json::parse(draft.serialized_entries);
    RequireValid(ValidateDraft(*transaction, scope, entries));
    if (draft.active_revision == kMaxVersion) {
        throw Problem{ProblemKind::Conflict, "version_exhausted",
                      "This environment reached its maximum release revision."};
    }
    const int revision = draft.active_revision + 1;
    const auto serialized = entries.dump();
    const storage::ReleaseRecord release{revision, request.rollback_revision, action, actor.username,
                                         request.note};
    storage::InsertRelease(*transaction, scope, release, serialized);
    storage::ActivateRelease(*transaction, scope, revision, serialized);
    storage::RecordEntryTypes(*transaction, scope, serialized);
    storage::RecordOperation(*transaction, scope, actor.username, request.idempotency_key,
                             {parameters, revision});
    transaction->Commit();
    return {revision, std::move(entries)};
}
}
