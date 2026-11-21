#include "runtime.hpp"

namespace flipkiln::application {
Json EvaluateSnapshot(const Snapshot& snapshot, const EvaluationInput& input, const Scope& scope) {
    auto results = Evaluate(snapshot.entries, input.keys, input.context, scope.project, scope.environment);
    return {{"revision", snapshot.revision}, {"results", std::move(results)}};
}
}
