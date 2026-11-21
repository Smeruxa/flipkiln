#pragma once

#include "access.hpp"

namespace flipkiln::storage {
struct ReleaseRequest {
    int draft_version;
    std::optional<int> rollback_revision;
    std::string note;
    std::string idempotency_key;
};

Json State(Connection& connection, const Scope& scope);
Json Validation(Connection& connection, const Scope& scope, const Json& entries);
Json SaveDraft(Database& db, const Actor& actor, const Scope& scope, int version, const Json& entries);
Json History(Connection& connection, const Scope& scope);
Json ReleaseDetail(Connection& connection, const Scope& scope, int revision);
Json Publish(Database& db, const Actor& actor, const Scope& scope, const ReleaseRequest& request);
}
