#pragma once

#include <string>
#include <string_view>

namespace flipkiln {
std::string RandomToken();
std::string Digest(std::string_view value);
std::string HashPassword(std::string_view password);
bool VerifyPassword(std::string_view password, std::string_view hash);
}
