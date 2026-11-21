#pragma once

#include "storage/access.hpp"

namespace flipkiln::application {
struct ReleaseRequest {
    int draft_version;
    std::optional<int> rollback_revision;
    std::string note;
    std::string idempotency_key;
};

Json EnvironmentState(Connection& connection, const storage::Actor& actor, const Scope& scope);
Json ReleaseHistory(Connection& connection, const storage::Actor& actor, const Scope& scope);
Json ReleaseDetail(Connection& connection, const storage::Actor& actor, const Scope& scope, int revision);
Json ValidateDraft(Connection& connection, const Scope& scope, const Json& entries);
Json SaveDraft(Database& db, const storage::Actor& actor, const Scope& scope, int version,
               const Json& entries);
Snapshot Publish(Database& db, const storage::Actor& actor, const Scope& scope,
                 const ReleaseRequest& request);
}
