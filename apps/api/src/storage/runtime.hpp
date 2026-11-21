#pragma once

#include "domain.hpp"
#include "scope.hpp"

namespace flipkiln::storage {
Json AuthorizedSnapshot(Connection& connection, const Scope& scope, std::string_view token);
void CheckRuntimeSchema(Connection& connection);
}
