#ifndef LOCALHIVE_STORAGE_METADATA_STORE_HPP
#define LOCALHIVE_STORAGE_METADATA_STORE_HPP

#include "localhive/core/types.hpp"
#include "localhive/storage/storage_layout.hpp"
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace localhive::storage {

struct BucketRecord {
    BucketId id{};
    std::string name;
};

struct ObjectRecord {
    ObjectId id{};
    BucketId bucket_id{};
    std::string key;
    std::optional<GenerationId> active_generation_id;
};

struct GenerationRecord {
    ObjectIdentity identity;
    GenerationState state{GenerationState::Receiving};
    uint64_t content_length{};
    std::string checksum_sha256;
};

struct RecoveryCandidate {
    ObjectIdentity identity;
    GenerationState state{GenerationState::Receiving};
};

class IMetadataStore {
public:
    virtual ~IMetadataStore() = default;
    virtual core::Status initialize() = 0;
    virtual core::Result<BucketRecord> create_bucket(std::string name) = 0;
    virtual core::Result<BucketRecord> find_bucket(std::string_view name) = 0;
    virtual core::Result<ObjectRecord> find_or_create_object(
        BucketId bucket_id, std::string_view key) = 0;
    virtual core::Status create_generation(const GenerationRecord& generation) = 0;
    virtual core::Status set_generation_state(GenerationId generation_id,
                                              GenerationState state) = 0;
    virtual core::Status stage_generation(GenerationId generation_id,
                                          uint64_t content_length,
                                          std::string_view checksum_sha256) = 0;
    // Makes generation visible and retires the previous active generation in
    // one SQLite transaction. The generation file must already exist on disk.
    virtual core::Status publish_generation(GenerationId generation_id) = 0;
    virtual core::Result<GenerationRecord> active_generation(
        BucketId bucket_id, std::string_view key) = 0;
    // Removes an object's active pointer and marks its generation reclaimable
    // in one metadata transaction. Returns NotFound if no active object exists.
    virtual core::Status delete_active_object(BucketId bucket_id,
                                              std::string_view key) = 0;
    virtual core::Result<std::vector<RecoveryCandidate>> recovery_candidates() = 0;
    virtual core::Result<std::vector<RecoveryCandidate>> reclaimable_generations() = 0;
    virtual core::Status remove_generation(GenerationId generation_id) = 0;
};

class SqliteMetadataStore final : public IMetadataStore {
public:
    explicit SqliteMetadataStore(std::filesystem::path database_path,
                                 std::string synchronous_mode = "NORMAL");
    ~SqliteMetadataStore() override;

    SqliteMetadataStore(const SqliteMetadataStore&) = delete;
    SqliteMetadataStore& operator=(const SqliteMetadataStore&) = delete;

    core::Status initialize() override;
    core::Result<BucketRecord> create_bucket(std::string name) override;
    core::Result<BucketRecord> find_bucket(std::string_view name) override;
    core::Result<ObjectRecord> find_or_create_object(
        BucketId bucket_id, std::string_view key) override;
    core::Status create_generation(const GenerationRecord& generation) override;
    core::Status set_generation_state(GenerationId generation_id,
                                      GenerationState state) override;
    core::Status stage_generation(GenerationId generation_id,
                                  uint64_t content_length,
                                  std::string_view checksum_sha256) override;
    core::Status publish_generation(GenerationId generation_id) override;
    core::Result<GenerationRecord> active_generation(
        BucketId bucket_id, std::string_view key) override;
    core::Status delete_active_object(BucketId bucket_id,
                                      std::string_view key) override;
    core::Result<std::vector<RecoveryCandidate>> recovery_candidates() override;
    core::Result<std::vector<RecoveryCandidate>> reclaimable_generations() override;
    core::Status remove_generation(GenerationId generation_id) override;

private:
    core::Status ensure_open_locked();
    std::filesystem::path database_path_;
    std::string synchronous_mode_;
    void* database_{nullptr};
    std::mutex mutex_;
};

} // namespace localhive::storage

#endif // LOCALHIVE_STORAGE_METADATA_STORE_HPP
