#include "localhive/storage/storage_layout.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace localhive::storage {
namespace {

std::string bucket_directory_name(BucketId bucket_id) {
    std::ostringstream stream;
    stream << "b_" << std::setw(8) << std::setfill('0') << bucket_id;
    return stream.str();
}

std::string generation_hex(GenerationId generation_id) {
    std::ostringstream stream;
    stream << std::hex << generation_id;
    return stream.str();
}

} // namespace

StorageLayout::StorageLayout(std::filesystem::path root) : root_(std::move(root)) {}

core::Status StorageLayout::initialize() const {
    if (root_.empty()) {
        return core::Status::Error(core::StatusCode::InvalidArgument,
                                   "Storage root cannot be empty");
    }
    std::error_code error;
    std::filesystem::create_directories(root_ / ".staging", error);
    if (error) {
        return core::Status::Error(core::StatusCode::Internal,
                                   "Could not create staging directory", error.message());
    }
    std::filesystem::create_directories(root_ / "data", error);
    if (error) {
        return core::Status::Error(core::StatusCode::Internal,
                                   "Could not create data directory", error.message());
    }
    return core::Status::Ok();
}

std::filesystem::path StorageLayout::metadata_path() const { return root_ / "metadata.db"; }

std::filesystem::path StorageLayout::staging_directory(BucketId bucket_id) const {
    return root_ / ".staging" / bucket_directory_name(bucket_id);
}

std::filesystem::path StorageLayout::data_directory(BucketId bucket_id) const {
    return root_ / "data" / bucket_directory_name(bucket_id);
}

std::filesystem::path StorageLayout::staging_path(BucketId bucket_id,
                                                   GenerationId generation_id) const {
    return staging_directory(bucket_id) / ("g_" + generation_hex(generation_id) + ".tmp");
}

std::filesystem::path StorageLayout::object_path(const ObjectIdentity& identity) const {
    return data_directory(identity.bucket_id) /
        (std::to_string(identity.object_id) + "_" + generation_hex(identity.generation_id) + ".dat");
}

core::Result<std::string> normalize_object_key(std::string_view key) {
    if (key.empty() || key.size() > 1024) {
        return core::Status::Error(core::StatusCode::InvalidArgument,
                                   "Object key must contain 1 to 1024 bytes");
    }
    std::string result;
    result.reserve(key.size());
    bool previous_separator = false;
    for (const char character : key) {
        if (character == '\0') {
            return core::Status::Error(core::StatusCode::InvalidArgument,
                                       "Object key cannot contain a NUL byte");
        }
        const char normalized = character == '\\' ? '/' : character;
        if (normalized == '/') {
            if (!previous_separator) result.push_back(normalized);
            previous_separator = true;
        } else {
            result.push_back(normalized);
            previous_separator = false;
        }
    }
    return result;
}

GenerationId GenerationIdGenerator::next() noexcept {
    constexpr GenerationId sequence_mask = (GenerationId{1} << 20) - 1;
    const auto epoch_ms = static_cast<GenerationId>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    GenerationId observed = last_.load(std::memory_order_relaxed);
    for (;;) {
        const GenerationId time_based = epoch_ms << 20;
        const GenerationId candidate = time_based > observed
            ? time_based
            : observed + 1;
        // A millisecond can issue at most 2^20 distinct generation IDs.
        // If that range overflows, advancing to the next encoded millisecond
        // still preserves ordering.
        const GenerationId adjusted = (candidate & sequence_mask) == sequence_mask
            ? ((candidate >> 20) + 1) << 20
            : candidate;
        if (last_.compare_exchange_weak(observed, adjusted,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {
            return adjusted;
        }
    }
}

} // namespace localhive::storage
