#ifndef LOCALHIVE_CORE_PLATFORM_HPP
#define LOCALHIVE_CORE_PLATFORM_HPP

#include "localhive/core/types.hpp"
#include <string>
#include <string_view>
#include <cstdint>
#include <filesystem>

namespace localhive::core::platform {

enum class OperatingSystem {
    Windows,
    Linux,
    MacOS,
    Unknown
};

// 1. Platform & Toolchain Identification
[[nodiscard]] constexpr OperatingSystem current_os() noexcept {
#if defined(_WIN32) || defined(_WIN64)
    return OperatingSystem::Windows;
#elif defined(__linux__)
    return OperatingSystem::Linux;
#elif defined(__APPLE__) && defined(__MACH__)
    return OperatingSystem::MacOS;
#else
    return OperatingSystem::Unknown;
#endif
}

[[nodiscard]] constexpr bool is_windows() noexcept {
    return current_os() == OperatingSystem::Windows;
}

[[nodiscard]] constexpr bool is_linux() noexcept {
    return current_os() == OperatingSystem::Linux;
}

[[nodiscard]] std::string_view os_name() noexcept;
[[nodiscard]] std::string_view compiler_name() noexcept;

// 2. High-Resolution Monotonic Clock
[[nodiscard]] uint64_t monotonic_now_ns() noexcept;
[[nodiscard]] uint64_t monotonic_now_ms() noexcept;

// 3. Path & Filesystem Helpers
// Normalizes path separators to forward slashes and resolves relative dots
[[nodiscard]] std::string normalize_path_string(std::string_view path);

// Forces hardware disk cache flush for an open file stream or path
Status flush_file_buffers(int native_fd);
Status flush_file_buffers(void* os_handle);

// Forces metadata flush for parent directory (e.g. post-rename)
Status sync_parent_directory(const std::filesystem::path& file_path);

// Atomically moves/replaces source file to destination path.
// Fails with explicit error if unsupported across filesystem boundaries.
Status atomic_replace_file(
    const std::filesystem::path& source,
    const std::filesystem::path& destination);

// Platform-aware deletion of files that might be open by concurrent readers
Status safe_delete_file(const std::filesystem::path& path);

} // namespace localhive::core::platform

#endif // LOCALHIVE_CORE_PLATFORM_HPP
