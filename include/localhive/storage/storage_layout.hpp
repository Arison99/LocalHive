#ifndef LOCALHIVE_STORAGE_STORAGE_LAYOUT_HPP
#define LOCALHIVE_STORAGE_STORAGE_LAYOUT_HPP

#include "localhive/core/types.hpp"
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace localhive::storage {

using BucketId = uint64_t;
using ObjectId = uint64_t;
using GenerationId = uint64_t;

enum class GenerationState {
    Receiving,
    Staged,
    Prepared,
    Published,
    Committed,
    Obsolete,
    Tombstone,
    Aborted
};

struct ObjectIdentity {
    BucketId bucket_id{};
    ObjectId object_id{};
    GenerationId generation_id{};
};

// The on-disk namespace contains only numeric identifiers. Client controlled
// bucket names and object keys must never be used as filesystem path segments.
class StorageLayout {
public:
    explicit StorageLayout(std::filesystem::path root);

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }
    [[nodiscard]] core::Status initialize() const;

    [[nodiscard]] std::filesystem::path metadata_path() const;
    [[nodiscard]] std::filesystem::path staging_directory(BucketId bucket_id) const;
    [[nodiscard]] std::filesystem::path data_directory(BucketId bucket_id) const;
    [[nodiscard]] std::filesystem::path staging_path(BucketId bucket_id,
                                                     GenerationId generation_id) const;
    [[nodiscard]] std::filesystem::path object_path(const ObjectIdentity& identity) const;

private:
    std::filesystem::path root_;
};

// Validates and normalizes a logical object key without resolving it as a path.
[[nodiscard]] core::Result<std::string> normalize_object_key(std::string_view key);

// Produces monotonically increasing IDs of the form (epoch_ms << 20) | sequence.
class GenerationIdGenerator {
public:
    [[nodiscard]] GenerationId next() noexcept;

private:
    std::atomic<GenerationId> last_{0};
};

} // namespace localhive::storage

#endif // LOCALHIVE_STORAGE_STORAGE_LAYOUT_HPP
