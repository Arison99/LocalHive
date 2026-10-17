#include "localhive/core/crypto/crypto.hpp"
#include <iostream>
#include <cassert>
#include <vector>

using namespace localhive::core::crypto;

void test_sha256_rfc6234_vectors() {
    std::cout << "[Test] SHA-256 RFC 6234 test vectors..." << std::endl;

    // Test 1: Empty string
    std::string h1 = sha256_hex("");
    assert(h1 == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // Test 2: "abc"
    std::string h2 = sha256_hex("abc");
    assert(h2 == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    // Test 3: "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
    std::string h3 = sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");
    assert(h3 == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    std::cout << "  ✓ All SHA-256 RFC 6234 test vectors matched exactly." << std::endl;
}

void test_hmac_sha256_rfc4231_vectors() {
    std::cout << "[Test] HMAC-SHA256 RFC 4231 test vectors..." << std::endl;

    // Test Vector 1: Key = 20 bytes 0x0b, Data = "Hi There"
    std::vector<uint8_t> key1(20, 0x0b);
    std::string data1 = "Hi There";
    auto digest1 = hmac_sha256(key1.data(), key1.size(), reinterpret_cast<const uint8_t*>(data1.data()), data1.size());
    std::string hex1 = to_hex(digest1.data(), digest1.size());
    assert(hex1 == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");

    // Test Vector 2: Key = "Jefe", Data = "what do ya want for nothing?"
    std::string hex2 = hmac_sha256_hex("Jefe", "what do ya want for nothing?");
    assert(hex2 == "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");

    std::cout << "  ✓ All HMAC-SHA256 RFC 4231 test vectors matched exactly." << std::endl;
}

void test_constant_time_equals() {
    std::cout << "[Test] Constant-time equality..." << std::endl;
    std::string a = "secret_signature_123456";
    std::string b = "secret_signature_123456";
    std::string c = "secret_signature_123459"; // last byte differs
    std::string d = "different_length";

    assert(constant_time_equals(a, b) == true);
    assert(constant_time_equals(a, c) == false);
    assert(constant_time_equals(a, d) == false);

    std::cout << "  ✓ Constant-time equality verified." << std::endl;
}

int main() {
    std::cout << "=== Running Crypto Tests ===" << std::endl;
    test_sha256_rfc6234_vectors();
    test_hmac_sha256_rfc4231_vectors();
    test_constant_time_equals();
    std::cout << "=== All Crypto Tests Passed! ===\n" << std::endl;
    return 0;
}
