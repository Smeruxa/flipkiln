#pragma once

#include "db.hpp"
#include "domain.hpp"

namespace flipkiln::storage {
inline Json ReadJson(const Rows& rows, std::size_t column = 0) {
    return Json::parse(rows.at(0).at(column).value());
}

inline Json ReadArray(const Rows& rows) {
    auto result = Json::array();
    for (const auto& row : rows) {
        result.push_back(Json::parse(row.at(0).value()));
    }
    return result;
}
}
