#pragma once

#include <string_view>

namespace flipkiln {
void Migrate();
void CreateUser(std::string_view username, std::string_view password);
}
