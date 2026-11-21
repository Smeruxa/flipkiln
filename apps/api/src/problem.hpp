#pragma once

#include <string_view>

#include "domain.hpp"

namespace flipkiln {
enum class ProblemKind {
    Invalid,
    Unauthenticated,
    Forbidden,
    Missing,
    Conflict,
    InvalidEntries,
    Throttled,
    PayloadTooLarge,
    UnsupportedMedia,
};

struct Problem {
    ProblemKind kind;
    std::string_view code;
    std::string_view message;
    Json issues = nullptr;
};

[[noreturn]] inline void Bad(std::string_view message) {
    throw Problem{ProblemKind::Invalid, "invalid_request", message};
}

[[noreturn]] inline void Unauthorized() {
    throw Problem{ProblemKind::Unauthenticated, "unauthorized", "Authentication required."};
}

[[noreturn]] inline void Forbidden() {
    throw Problem{ProblemKind::Forbidden, "forbidden", "Your role does not allow this operation."};
}

[[noreturn]] inline void Missing() {
    throw Problem{ProblemKind::Missing, "not_found", "The requested resource was not found."};
}

[[noreturn]] inline void DraftConflict() {
    throw Problem{ProblemKind::Conflict, "draft_conflict",
                  "The draft changed. Reload and review before trying again."};
}

inline void RequireValid(Json issues) {
    if (!issues.empty()) {
        throw Problem{ProblemKind::InvalidEntries, "validation_failed",
                      "The configuration contains invalid entries.", std::move(issues)};
    }
}
}
