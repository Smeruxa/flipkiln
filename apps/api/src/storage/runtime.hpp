#pragma once

#include "scope.hpp"

namespace flipkiln::storage {
std::optional<Snapshot> FindClientSnapshot(Connection& connection, const Scope& scope,
                                           std::string_view token_digest);
void CheckRuntimeSchema(Connection& connection);
}
