#include "runtime.hpp"

#include "json_rows.hpp"

namespace flipkiln::storage {
namespace {
constexpr std::string_view kReader = R"(
    SELECT revision::text, entries::text
    FROM public.flipkiln_client_snapshot($1, $2, $3)
)";
}

std::optional<Snapshot> FindClientSnapshot(Connection& connection, const Scope& scope,
                                           std::string_view token_digest) {
    const auto rows =
        connection.Query(kReader, {scope.project, scope.environment, std::string(token_digest)});
    if (rows.empty()) {
        return std::nullopt;
    }
    return Snapshot{std::stoi(rows.front().at(0).value()), ReadJson(rows, 1)};
}

void CheckRuntimeSchema(Connection& connection) { connection.Query(kReader, {"", "", ""}); }
}
