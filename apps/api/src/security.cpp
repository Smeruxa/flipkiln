#include "security.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <array>
#include <span>
#include <stdexcept>

namespace flipkiln {
namespace {
std::string Hex(std::span<const unsigned char> bytes) {
    constexpr std::string_view digits{"0123456789abcdef"};
    std::string result;
    result.reserve(bytes.size() * 2);
    for (const auto value : bytes) {
        result += digits[value >> 4];
        result += digits[value & 15];
    }
    return result;
}

std::string PasswordHash(std::string_view password, std::string_view salt) {
    std::array<unsigned char, 32> result;
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                          reinterpret_cast<const unsigned char*>(salt.data()), static_cast<int>(salt.size()),
                          600000, EVP_sha256(), static_cast<int>(result.size()), result.data()) != 1) {
        throw std::runtime_error("PBKDF2 failed");
    }
    return Hex(result);
}
}

std::string RandomToken() {
    std::array<unsigned char, 32> bytes;
    if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
        throw std::runtime_error("Cannot generate secure random bytes");
    }
    return Hex(bytes);
}

std::string Digest(std::string_view value) {
    std::array<unsigned char, 32> result;
    unsigned int length{};
    if (EVP_Digest(value.data(), value.size(), result.data(), &length, EVP_sha256(), nullptr) != 1 ||
        length != result.size()) {
        throw std::runtime_error("Cannot compute SHA-256");
    }
    return Hex(result);
}

std::string HashPassword(std::string_view password) {
    const auto salt = RandomToken().substr(0, 32);
    return salt + ":" + PasswordHash(password, salt);
}

bool VerifyPassword(std::string_view password, std::string_view hash) {
    if (hash.length() != 97 || hash[32] != ':') {
        return false;
    }
    const auto actual = PasswordHash(password, hash.substr(0, 32));
    return CRYPTO_memcmp(actual.data(), hash.data() + 33, 64) == 0;
}
}
