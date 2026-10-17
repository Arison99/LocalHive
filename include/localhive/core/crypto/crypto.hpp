#ifndef LOCALHIVE_CORE_CRYPTO_HPP
#define LOCALHIVE_CORE_CRYPTO_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <array>
#include <vector>

namespace localhive::core::crypto {

// Computes 32-byte binary SHA-256 digest
[[nodiscard]] std::array<uint8_t, 32> sha256(const uint8_t* data, size_t length) noexcept;
[[nodiscard]] std::array<uint8_t, 32> sha256(std::string_view input) noexcept;

// Computes lowercase hexadecimal representation of SHA-256 digest
[[nodiscard]] std::string sha256_hex(const uint8_t* data, size_t length);
[[nodiscard]] std::string sha256_hex(std::string_view input);

// Computes 32-byte HMAC-SHA256 (RFC 2104)
[[nodiscard]] std::array<uint8_t, 32> hmac_sha256(
    const uint8_t* key, size_t key_len,
    const uint8_t* data, size_t data_len) noexcept;

[[nodiscard]] std::array<uint8_t, 32> hmac_sha256(
    std::string_view key,
    std::string_view data) noexcept;

// Computes lowercase hex HMAC-SHA256
[[nodiscard]] std::string hmac_sha256_hex(
    std::string_view key,
    std::string_view data);

// Timing-safe byte comparison to eliminate side-channel vulnerabilities
[[nodiscard]] bool constant_time_equals(
    const uint8_t* a, const uint8_t* b, size_t length) noexcept;

[[nodiscard]] bool constant_time_equals(
    std::string_view a, std::string_view b) noexcept;

// Hexadecimal encoder
[[nodiscard]] std::string to_hex(const uint8_t* data, size_t length);

} // namespace localhive::core::crypto

#endif // LOCALHIVE_CORE_CRYPTO_HPP
