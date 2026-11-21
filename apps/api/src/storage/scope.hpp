#pragma once

#include "db.hpp"
#include "domain.hpp"

namespace flipkiln::storage {
inline Params ScopeParameters(const Scope& scope) { return {scope.project, scope.environment}; }
}
