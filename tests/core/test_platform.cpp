#include "localhive/core/platform/platform.hpp"
#include <iostream>
#include <cassert>
#include <fstream>

using namespace localhive::core;
using namespace localhive::core::platform;

void test_os_and_compiler_identification() {
    std::cout << "[Test] OS and compiler identification..." << std::endl;
    auto os = current_os();
    assert(os != OperatingSystem::Unknown);
    assert(!os_name().empty());
    assert(!compiler_name().empty());

#if defined(_WIN32)
    assert(is_windows() == true);
    assert(is_linux() == false);
    assert(os_name() == "Windows");
#elif defined(__linux__)
    assert(is_linux() == true);
    assert(is_windows() == false);
    assert(os_name() == "Linux");
#endif
    std::cout << "  ✓ Running on: " << os_name() << " (" << compiler_name() << ")" << std::endl;
}

void test_monotonic_clock() {
    std::cout << "[Test] Monotonic clock..." << std::endl;
    uint64_t t1 = monotonic_now_ns();
    uint64_t t1_ms = monotonic_now_ms();
    
    // Busy wait a few iterations
    volatile int sink = 0;
    for (int i = 0; i < 100000; ++i) { sink += i; }

    uint64_t t2 = monotonic_now_ns();
    uint64_t t2_ms = monotonic_now_ms();

    assert(t2 >= t1);
    assert(t2_ms >= t1_ms);
    std::cout << "  ✓ Clock monotonically increased (" << (t2 - t1) << " ns)" << std::endl;
}

void test_path_normalization() {
    std::cout << "[Test] Path normalization..." << std::endl;
    std::string win_path = "buckets\\my-bucket\\objects\\file.txt";
    std::string normalized = normalize_path_string(win_path);
    assert(normalized == "buckets/my-bucket/objects/file.txt");
    std::cout << "  ✓ Path normalized to: " << normalized << std::endl;
}

void test_atomic_replace_and_delete() {
    std::cout << "[Test] Atomic replace and safe delete..." << std::endl;
    std::filesystem::path test_dir = "test_scratch_platform";
    std::filesystem::create_directories(test_dir);

    std::filesystem::path src = test_dir / "temp_file.tmp";
    std::filesystem::path dst = test_dir / "active_file.dat";

    // Write source file
    {
        std::ofstream out(src);
        out << "Hello Atomic Storage!";
    }
    assert(std::filesystem::exists(src));

    // Atomically move/replace to destination
    auto st = atomic_replace_file(src, dst);
    assert(st.ok());
    assert(!std::filesystem::exists(src));
    assert(std::filesystem::exists(dst));

    // Verify content
    {
        std::ifstream in(dst);
        std::string content;
        std::getline(in, content);
        assert(content == "Hello Atomic Storage!");
    }

    // Safe delete
    auto del_st = safe_delete_file(dst);
    assert(del_st.ok());
    assert(!std::filesystem::exists(dst));

    std::filesystem::remove_all(test_dir);
    std::cout << "  ✓ Atomic replace and delete verified." << std::endl;
}

int main() {
    std::cout << "=== Running Platform Tests ===" << std::endl;
    test_os_and_compiler_identification();
    test_monotonic_clock();
    test_path_normalization();
    test_atomic_replace_and_delete();
    std::cout << "=== All Platform Tests Passed! ===\n" << std::endl;
    return 0;
}
