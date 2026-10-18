#include "localhive/storage/object_store.hpp"

#include "localhive/core/platform/platform.hpp"
#include <system_error>

#if defined(_WIN32) || defined(_WIN64)
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace localhive::storage {
namespace {

core::Status sync_path(const std::filesystem::path& path) {
#if defined(_WIN32) || defined(_WIN64)
    const int descriptor = _open(path.string().c_str(), _O_RDONLY | _O_BINARY);
#else
    const int descriptor = ::open(path.c_str(), O_RDONLY);
#endif
    if (descriptor < 0) {
        return core::Status::Error(core::StatusCode::Internal,
                                   "Could not open staged file for sync");
    }
    const auto status = core::platform::flush_file_buffers(descriptor);
#if defined(_WIN32) || defined(_WIN64)
    _close(descriptor);
#else
    ::close(descriptor);
#endif
    return status;
}

} // namespace

ObjectWriteSession::ObjectWriteSession(IMetadataStore* metadata, StorageLayout layout,
                                       ObjectIdentity identity,
                                       core::config::DurabilityProfile durability)
    : metadata_(metadata), layout_(std::move(layout)), identity_(identity), durability_(durability) {}

ObjectWriteSession::~ObjectWriteSession() { abort(); }

ObjectWriteSession::ObjectWriteSession(ObjectWriteSession&& other) noexcept
    : metadata_(other.metadata_), layout_(std::move(other.layout_)), identity_(other.identity_),
      durability_(other.durability_), stream_(std::move(other.stream_)),
      bytes_written_(other.bytes_written_), finished_(other.finished_) {
    other.metadata_ = nullptr;
    other.finished_ = true;
}

ObjectWriteSession& ObjectWriteSession::operator=(ObjectWriteSession&& other) noexcept {
    if (this == &other) return *this;
    abort();
    metadata_ = other.metadata_; layout_ = std::move(other.layout_); identity_ = other.identity_;
    durability_ = other.durability_; stream_ = std::move(other.stream_);
    bytes_written_ = other.bytes_written_; finished_ = other.finished_;
    other.metadata_ = nullptr; other.finished_ = true;
    return *this;
}

core::Status ObjectWriteSession::open() {
    std::error_code error;
    std::filesystem::create_directories(layout_.staging_directory(identity_.bucket_id), error);
    if (error) return core::Status::Error(core::StatusCode::Internal, "Could not create staging directory", error.message());
    stream_.open(layout_.staging_path(identity_.bucket_id, identity_.generation_id),
                 std::ios::binary | std::ios::trunc);
    if (!stream_) return core::Status::Error(core::StatusCode::Internal, "Could not open staging file");
    return core::Status::Ok();
}

core::Status ObjectWriteSession::write(const uint8_t* data, size_t length) {
    if (finished_ || metadata_ == nullptr) return core::Status::Error(core::StatusCode::FailedPrecondition, "Write session is closed");
    if (data == nullptr && length != 0) return core::Status::Error(core::StatusCode::InvalidArgument, "Data cannot be null when length is nonzero");
    stream_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(length));
    if (!stream_) return core::Status::Error(core::StatusCode::Internal, "Could not write staging file");
    bytes_written_ += length;
    return core::Status::Ok();
}

core::Result<ObjectIdentity> ObjectWriteSession::finish(std::string checksum_sha256) {
    if (finished_ || metadata_ == nullptr) return core::Status::Error(core::StatusCode::FailedPrecondition, "Write session is closed");
    stream_.flush();
    stream_.close();
    const auto staging = layout_.staging_path(identity_.bucket_id, identity_.generation_id);
    // The payload reaches stable storage before its metadata can become visible.
    if (durability_ != core::config::DurabilityProfile::Development) {
        auto status = sync_path(staging); if (!status.ok()) { abort(); return status; }
    }
    auto status = metadata_->stage_generation(identity_.generation_id, bytes_written_, checksum_sha256);
    if (!status.ok()) { abort(); return status; }
    std::error_code error;
    std::filesystem::create_directories(layout_.data_directory(identity_.bucket_id), error);
    if (error) { abort(); return core::Status::Error(core::StatusCode::Internal, "Could not create data directory", error.message()); }
    const auto final_path = layout_.object_path(identity_);
    // Each generation has a unique final name, so this move never overwrites
    // an existing generation or damages readers of the prior version.
    status = core::platform::atomic_replace_file(staging, final_path);
    if (!status.ok()) { abort(); return status; }
    if (durability_ == core::config::DurabilityProfile::Strict) {
        status = core::platform::sync_parent_directory(final_path);
        if (!status.ok()) return status;
    }
    status = metadata_->set_generation_state(identity_.generation_id, GenerationState::Prepared);
    if (!status.ok()) return status;
    // Publication is the visibility boundary: SQLite changes the active
    // generation only after the immutable file already exists.
    status = metadata_->publish_generation(identity_.generation_id);
    if (!status.ok()) return status;
    finished_ = true;
    return identity_;
}

void ObjectWriteSession::abort() noexcept {
    if (finished_) return;
    if (stream_.is_open()) stream_.close();
    std::error_code ignored;
    std::filesystem::remove(layout_.staging_path(identity_.bucket_id, identity_.generation_id), ignored);
    if (metadata_ != nullptr) {
        try { (void)metadata_->set_generation_state(identity_.generation_id, GenerationState::Aborted); }
        catch (...) { }
    }
    finished_ = true;
}

ObjectStore::ObjectStore(StorageLayout layout, std::shared_ptr<IMetadataStore> metadata,
                         core::config::DurabilityProfile durability)
    : layout_(std::move(layout)), metadata_(std::move(metadata)), durability_(durability) {}

core::Result<size_t> ObjectStore::ObjectReadSession::read(uint8_t* destination, size_t capacity) {
    if (destination == nullptr && capacity != 0) {
        return core::Status::Error(core::StatusCode::InvalidArgument,
                                   "Destination cannot be null when capacity is nonzero");
    }
    stream_.read(reinterpret_cast<char*>(destination), static_cast<std::streamsize>(capacity));
    const auto count = static_cast<size_t>(stream_.gcount());
    if (stream_.bad()) return core::Status::Error(core::StatusCode::Internal, "Could not read object data");
    return count;
}

core::Status ObjectStore::initialize() {
    if (!metadata_) return core::Status::Error(core::StatusCode::InvalidArgument, "Metadata store cannot be null");
    auto status = layout_.initialize();
    if (!status.ok()) return status;
    return metadata_->initialize();
}

core::Result<ObjectWriteSession> ObjectStore::open_writer(BucketId bucket_id, std::string_view key) {
    if (!metadata_) return core::Status::Error(core::StatusCode::FailedPrecondition, "Object store is not configured");
    auto object = metadata_->find_or_create_object(bucket_id, key);
    if (!object.ok()) return object.status();
    ObjectIdentity identity{bucket_id, object.value().id, generations_.next()};
    auto status = metadata_->create_generation({identity, GenerationState::Receiving, 0, ""});
    if (!status.ok()) return status;
    ObjectWriteSession session(metadata_.get(), layout_, identity, durability_);
    status = session.open();
    if (!status.ok()) { metadata_->set_generation_state(identity.generation_id, GenerationState::Aborted); return status; }
    return std::move(session);
}

core::Result<ObjectStore::ObjectReadSession> ObjectStore::open_reader(
    BucketId bucket_id, std::string_view key) {
    if (!metadata_) return core::Status::Error(core::StatusCode::FailedPrecondition, "Object store is not configured");
    auto generation = metadata_->active_generation(bucket_id, key);
    if (!generation.ok()) return generation.status();
    const auto path = layout_.object_path(generation.value().identity);
    ReaderLease lease = reader_leases_->acquire(generation.value().identity.generation_id);
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return core::Status::Error(core::StatusCode::NotFound,
        "Active generation data file does not exist", path.string());
    return ObjectReadSession(generation.value(), std::move(lease), std::move(stream));
}

core::Status ObjectStore::delete_object(BucketId bucket_id, std::string_view key) {
    if (!metadata_) {
        return core::Status::Error(core::StatusCode::FailedPrecondition,
                                   "Object store is not configured");
    }
    return metadata_->delete_active_object(bucket_id, key);
}

} // namespace localhive::storage
