#include "runtime.hpp"

#include "json_rows.hpp"
#include "problem.hpp"
#include "security.hpp"

namespace flipkiln::storage {
namespace {
constexpr std::string_view kReader = R"(
    SELECT revision::text, entries::text
    FROM public.flipkiln_client_snapshot($1, $2, $3)
)";
}

Json AuthorizedSnapshot(Connection& connection, const Scope& scope, std::string_view token) {
    const auto rows = connection.Query(kReader, {scope.project, scope.environment, Digest(token)});
    if (rows.empty()) {
        Unauthorized();
    }
    return {{"revision", std::stoi(rows.at(0).at(0).value())}, {"entries", ReadJson(rows, 1)}};
}

void CheckRuntimeSchema(Connection& connection) { connection.Query(kReader, {"", "", ""}); }
}
