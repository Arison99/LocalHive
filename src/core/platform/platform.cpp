#include "localhive/core/platform/platform.hpp"
#include <chrono>
#include <system_error>
#include <algorithm>

#if defined(_WIN32) || defined(_WIN64)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <io.h>
#else
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

namespace localhive::core::platform {

std::string_view os_name() noexcept {
#if defined(_WIN32) || defined(_WIN64)
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#elif defined(__APPLE__)
    return "macOS";
#else
    return "Unknown";
#endif
}

std::string_view compiler_name() noexcept {
#if defined(_MSC_VER)
    return "MSVC";
#elif defined(__clang__)
    return "Clang";
#elif defined(__GNUC__)
    return "GCC";
#else
    return "Unknown Compiler";
#endif
}

uint64_t monotonic_now_ns() noexcept {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
}

uint64_t monotonic_now_ms() noexcept {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::string normalize_path_string(std::string_view path) {
    std::string result(path);
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

Status flush_file_buffers(int native_fd) {
    if (native_fd < 0) {
        return Status::Error(StatusCode::InvalidArgument, "Invalid file descriptor for flush");
    }
#if defined(_WIN32) || defined(_WIN64)
    intptr_t os_handle = _get_osfhandle(native_fd);
    if (os_handle == -1) {
        return Status::Error(StatusCode::Internal, "Failed to retrieve OS handle from CRT descriptor");
    }
    return flush_file_buffers(reinterpret_cast<void*>(os_handle));
#else
    if (::fdatasync(native_fd) != 0) {
        return Status::Error(StatusCode::Internal, "fdatasync failed", std::strerror(errno));
    }
    return Status::Ok();
#endif
}

Status flush_file_buffers(void* os_handle) {
#if defined(_WIN32) || defined(_WIN64)
    if (os_handle == nullptr || os_handle == INVALID_HANDLE_VALUE) {
        return Status::Error(StatusCode::InvalidArgument, "Invalid Win32 HANDLE for FlushFileBuffers");
    }
    if (!::FlushFileBuffers(reinterpret_cast<HANDLE>(os_handle))) {
        DWORD err = ::GetLastError();
        return Status::Error(StatusCode::Internal, "FlushFileBuffers failed", std::to_string(err));
    }
    return Status::Ok();
#else
    (void)os_handle;
    return Status::Error(StatusCode::Unimplemented, "Win32 HANDLE flush not supported on POSIX platform");
#endif
}

Status sync_parent_directory(const std::filesystem::path& file_path) {
    std::filesystem::path parent = file_path.parent_path();
    if (parent.empty()) {
        parent = ".";
    }

#if defined(_WIN32) || defined(_WIN64)
    // On Windows NTFS, metadata for MoveFileEx with MOVEFILE_WRITE_THROUGH is committed synchronously.
    // Explicit directory flushing via handle requires FILE_FLAG_BACKUP_SEMANTICS.
    HANDLE hDir = ::CreateFileW(
        parent.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr
    );
    if (hDir == INVALID_HANDLE_VALUE) {
        // Not a fatal failure if directory handle cannot be opened for flush
        return Status::Ok();
    }
    ::FlushFileBuffers(hDir);
    ::CloseHandle(hDir);
    return Status::Ok();
#else
    int dir_fd = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY);
    if (dir_fd < 0) {
        return Status::Error(StatusCode::Internal, "Failed to open parent directory for fsync", std::strerror(errno));
    }
    int rc = ::fsync(dir_fd);
    ::close(dir_fd);
    if (rc != 0) {
        return Status::Error(StatusCode::Internal, "fsync on parent directory failed", std::strerror(errno));
    }
    return Status::Ok();
#endif
}

Status atomic_replace_file(
    const std::filesystem::path& source,
    const std::filesystem::path& destination) {
    std::error_code ec;
    if (!std::filesystem::exists(source, ec)) {
        return Status::Error(StatusCode::NotFound, "Source file does not exist: " + source.string());
    }

#if defined(_WIN32) || defined(_WIN64)
    // On Windows, if destination does not exist, use MoveFileExW with MOVEFILE_WRITE_THROUGH.
    // If destination exists, use MoveFileExW with MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH.
    DWORD flags = MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH;
    if (!::MoveFileExW(source.c_str(), destination.c_str(), flags)) {
        DWORD err = ::GetLastError();
        return Status::Error(StatusCode::Internal, "MoveFileExW failed", std::to_string(err));
    }
    return Status::Ok();
#else
    // On Linux / POSIX, rename is atomic within the same filesystem
    if (::rename(source.c_str(), destination.c_str()) != 0) {
        return Status::Error(StatusCode::Internal, "rename failed", std::strerror(errno));
    }
    return Status::Ok();
#endif
}

Status safe_delete_file(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return Status::Ok(); // Idempotent: already gone
    }

#if defined(_WIN32) || defined(_WIN64)
    // On Windows, std::filesystem::remove fails if the file is held open by a reader unless
    // POSIX delete semantics were established.
    if (!std::filesystem::remove(path, ec)) {
        if (ec) {
            return Status::Error(StatusCode::Internal, "Failed to delete file: " + ec.message());
        }
    }
    return Status::Ok();
#else
    // POSIX unlink succeeds immediately even if open by readers
    if (::unlink(path.c_str()) != 0) {
        return Status::Error(StatusCode::Internal, "unlink failed", std::strerror(errno));
    }
    return Status::Ok();
#endif
}

} // namespace localhive::core::platform
