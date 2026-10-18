#ifndef LOCALHIVE_STORAGE_OBJECT_STORE_HPP
#define LOCALHIVE_STORAGE_OBJECT_STORE_HPP

#include "localhive/core/config/config.hpp"
#include "localhive/core/types.hpp"
#include "localhive/storage/metadata_store.hpp"
#include "localhive/storage/reader_leases.hpp"
#include "localhive/storage/storage_layout.hpp"
#include <filesystem>
#include <fstream>
#include <memory>

namespace localhive::storage {

class ObjectWriteSession {
public:
    ObjectWriteSession() = default;
    ~ObjectWriteSession();
    ObjectWriteSession(ObjectWriteSession&&) noexcept;
    ObjectWriteSession& operator=(ObjectWriteSession&&) noexcept;
    ObjectWriteSession(const ObjectWriteSession&) = delete;
    ObjectWriteSession& operator=(const ObjectWriteSession&) = delete;

    // Appends one chunk to the private staging file. The generation remains
    // invisible to readers until finish() publishes it.
    [[nodiscard]] core::Status write(const uint8_t* data, size_t length);
    // Publishes this generation. checksum_sha256 is persisted as supplied by
    // the caller after its streaming digest verification succeeds.
    [[nodiscard]] core::Result<ObjectIdentity> finish(std::string checksum_sha256);
    void abort() noexcept;

private:
    friend class ObjectStore;
    ObjectWriteSession(IMetadataStore* metadata, StorageLayout layout,
                       ObjectIdentity identity,
                       core::config::DurabilityProfile durability);
    core::Status open();

    IMetadataStore* metadata_{nullptr};
    StorageLayout layout_{std::filesystem::path{}};
    ObjectIdentity identity_{};
    core::config::DurabilityProfile durability_{core::config::DurabilityProfile::Standard};
    std::ofstream stream_;
    uint64_t bytes_written_{0};
    bool finished_{false};
};

class ObjectStore {
public:
    ObjectStore(StorageLayout layout, std::shared_ptr<IMetadataStore> metadata,
                core::config::DurabilityProfile durability);

    [[nodiscard]] core::Status initialize();
    // Starts a new immutable generation for an object. Concurrent writers get
    // separate staging files and the last successful publication becomes active.
    [[nodiscard]] core::Result<ObjectWriteSession> open_writer(
        BucketId bucket_id, std::string_view key);
    class ObjectReadSession {
    public:
        ObjectReadSession() = default;
        [[nodiscard]] core::Result<size_t> read(uint8_t* destination, size_t capacity);
        [[nodiscard]] const ObjectIdentity& identity() const noexcept { return identity_; }
        [[nodiscard]] uint64_t content_length() const noexcept { return content_length_; }
        [[nodiscard]] const std::string& checksum_sha256() const noexcept { return checksum_sha256_; }
    private:
        friend class ObjectStore;
        ObjectReadSession(const GenerationRecord& generation, ReaderLease lease, std::ifstream stream)
            : identity_(generation.identity), content_length_(generation.content_length),
              checksum_sha256_(generation.checksum_sha256), lease_(std::move(lease)),
              stream_(std::move(stream)) {}
        ObjectIdentity identity_{};
        uint64_t content_length_{0};
        std::string checksum_sha256_;
        ReaderLease lease_;
        std::ifstream stream_;
    };
    // Pins the active generation with a reader lease before opening its file.
    // Garbage collection therefore cannot remove data being streamed.
    [[nodiscard]] core::Result<ObjectReadSession> open_reader(
        BucketId bucket_id, std::string_view key);
    // Hides the active generation immediately; existing leased readers retain
    // access until their sessions close and garbage collection runs.
    [[nodiscard]] core::Status delete_object(BucketId bucket_id,
                                             std::string_view key);
    [[nodiscard]] ReaderLeaseManager& reader_leases() noexcept { return *reader_leases_; }

private:
    StorageLayout layout_;
    std::shared_ptr<IMetadataStore> metadata_;
    core::config::DurabilityProfile durability_;
    GenerationIdGenerator generations_;
    std::shared_ptr<ReaderLeaseManager> reader_leases_{std::make_shared<ReaderLeaseManager>()};
};

} // namespace localhive::storage

#endif // LOCALHIVE_STORAGE_OBJECT_STORE_HPP
