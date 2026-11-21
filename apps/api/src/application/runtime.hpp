#pragma once

#include "domain.hpp"

namespace flipkiln::application {
Json EvaluateSnapshot(const Snapshot& snapshot, const EvaluationInput& input, const Scope& scope);
}
