#include "localhive/core/crypto/crypto.hpp"
#include <cstring>

namespace localhive::core::crypto {

namespace {

// Right rotate 32-bit integer
constexpr inline uint32_t rotr(uint32_t x, uint32_t n) noexcept {
    return (x >> n) | (x << (32 - n));
}

// SHA-256 round functions
constexpr inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) noexcept {
    return (x & y) ^ (~x & z);
}

constexpr inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) noexcept {
    return (x & y) ^ (x & z) ^ (y & z);
}

constexpr inline uint32_t sigma0(uint32_t x) noexcept {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

constexpr inline uint32_t sigma1(uint32_t x) noexcept {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

constexpr inline uint32_t gamma0(uint32_t x) noexcept {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

constexpr inline uint32_t gamma1(uint32_t x) noexcept {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

// SHA-256 constants K
constexpr uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

struct Sha256Context {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
};

void sha256_init(Sha256Context& ctx) noexcept {
    ctx.state[0] = 0x6a09e667;
    ctx.state[1] = 0xbb67ae85;
    ctx.state[2] = 0x3c6ef372;
    ctx.state[3] = 0xa54ff53a;
    ctx.state[4] = 0x510e527f;
    ctx.state[5] = 0x9b05688c;
    ctx.state[6] = 0x1f83d9ab;
    ctx.state[7] = 0x5be0cd19;
    ctx.count = 0;
}

void sha256_transform(uint32_t state[8], const uint8_t data[64]) noexcept {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(data[i * 4]) << 24) |
               (static_cast<uint32_t>(data[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(data[i * 4 + 2]) << 8) |
               (static_cast<uint32_t>(data[i * 4 + 3]));
    }
    for (int i = 16; i < 64; ++i) {
        w[i] = gamma1(w[i - 2]) + w[i - 7] + gamma0(w[i - 15]) + w[i - 16];
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t t1 = h + sigma1(e) + ch(e, f, g) + K[i] + w[i];
        uint32_t t2 = sigma0(a) + maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void sha256_update(Sha256Context& ctx, const uint8_t* data, size_t len) noexcept {
    size_t index = static_cast<size_t>(ctx.count & 63);
    ctx.count += len;
    size_t part_len = 64 - index;

    size_t i = 0;
    if (len >= part_len) {
        std::memcpy(&ctx.buffer[index], data, part_len);
        sha256_transform(ctx.state, ctx.buffer);
        for (i = part_len; i + 63 < len; i += 64) {
            sha256_transform(ctx.state, &data[i]);
        }
        index = 0;
    }
    std::memcpy(&ctx.buffer[index], &data[i], len - i);
}

void sha256_final(Sha256Context& ctx, uint8_t digest[32]) noexcept {
    uint8_t bits[8];
    uint64_t total_bits = ctx.count * 8;
    for (int i = 0; i < 8; ++i) {
        bits[i] = static_cast<uint8_t>((total_bits >> ((7 - i) * 8)) & 0xff);
    }

    size_t index = static_cast<size_t>(ctx.count & 63);
    size_t pad_len = (index < 56) ? (56 - index) : (120 - index);

    static const uint8_t PADDING[64] = {
        0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    sha256_update(ctx, PADDING, pad_len);
    sha256_update(ctx, bits, 8);

    for (int i = 0; i < 8; ++i) {
        digest[i * 4] = static_cast<uint8_t>((ctx.state[i] >> 24) & 0xff);
        digest[i * 4 + 1] = static_cast<uint8_t>((ctx.state[i] >> 16) & 0xff);
        digest[i * 4 + 2] = static_cast<uint8_t>((ctx.state[i] >> 8) & 0xff);
        digest[i * 4 + 3] = static_cast<uint8_t>(ctx.state[i] & 0xff);
    }
}

} // anonymous namespace

std::array<uint8_t, 32> sha256(const uint8_t* data, size_t length) noexcept {
    Sha256Context ctx;
    sha256_init(ctx);
    if (data && length > 0) {
        sha256_update(ctx, data, length);
    }
    std::array<uint8_t, 32> digest;
    sha256_final(ctx, digest.data());
    return digest;
}

std::array<uint8_t, 32> sha256(std::string_view input) noexcept {
    return sha256(reinterpret_cast<const uint8_t*>(input.data()), input.size());
}

std::string sha256_hex(const uint8_t* data, size_t length) {
    auto digest = sha256(data, length);
    return to_hex(digest.data(), digest.size());
}

std::string sha256_hex(std::string_view input) {
    return sha256_hex(reinterpret_cast<const uint8_t*>(input.data()), input.size());
}

std::array<uint8_t, 32> hmac_sha256(
    const uint8_t* key, size_t key_len,
    const uint8_t* data, size_t data_len) noexcept {
    uint8_t k[64] = {0};
    if (key_len > 64) {
        auto key_hash = sha256(key, key_len);
        std::memcpy(k, key_hash.data(), 32);
    } else if (key && key_len > 0) {
        std::memcpy(k, key, key_len);
    }

    uint8_t k_ipad[64];
    uint8_t k_opad[64];
    for (int i = 0; i < 64; ++i) {
        k_ipad[i] = k[i] ^ 0x36;
        k_opad[i] = k[i] ^ 0x5c;
    }

    // Inner hash: SHA256(k_ipad || data)
    Sha256Context inner_ctx;
    sha256_init(inner_ctx);
    sha256_update(inner_ctx, k_ipad, 64);
    if (data && data_len > 0) {
        sha256_update(inner_ctx, data, data_len);
    }
    uint8_t inner_digest[32];
    sha256_final(inner_ctx, inner_digest);

    // Outer hash: SHA256(k_opad || inner_digest)
    Sha256Context outer_ctx;
    sha256_init(outer_ctx);
    sha256_update(outer_ctx, k_opad, 64);
    sha256_update(outer_ctx, inner_digest, 32);
    std::array<uint8_t, 32> outer_digest;
    sha256_final(outer_ctx, outer_digest.data());

    return outer_digest;
}

std::array<uint8_t, 32> hmac_sha256(std::string_view key, std::string_view data) noexcept {
    return hmac_sha256(
        reinterpret_cast<const uint8_t*>(key.data()), key.size(),
        reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

std::string hmac_sha256_hex(std::string_view key, std::string_view data) {
    auto digest = hmac_sha256(key, data);
    return to_hex(digest.data(), digest.size());
}

bool constant_time_equals(const uint8_t* a, const uint8_t* b, size_t length) noexcept {
    if (a == nullptr || b == nullptr) return a == b;
    uint8_t diff = 0;
    for (size_t i = 0; i < length; ++i) {
        diff |= (a[i] ^ b[i]);
    }
    return diff == 0;
}

bool constant_time_equals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    return constant_time_equals(
        reinterpret_cast<const uint8_t*>(a.data()),
        reinterpret_cast<const uint8_t*>(b.data()),
        a.size());
}

std::string to_hex(const uint8_t* data, size_t length) {
    static const char hex_chars[] = "0123456789abcdef";
    std::string out;
    out.reserve(length * 2);
    for (size_t i = 0; i < length; ++i) {
        out.push_back(hex_chars[(data[i] >> 4) & 0x0f]);
        out.push_back(hex_chars[data[i] & 0x0f]);
    }
    return out;
}

} // namespace localhive::core::crypto
