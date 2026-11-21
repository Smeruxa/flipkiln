#pragma once

#include "db.hpp"

namespace flipkiln::storage {
struct Scope {
    std::string project;
    std::string environment;

    Params Parameters() const { return {project, environment}; }
};
}
