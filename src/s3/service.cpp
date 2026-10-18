#include "localhive/s3/service.hpp"

#include <utility>

namespace localhive::s3 {

S3Service::S3Service(std::filesystem::path storage_root,
                     std::shared_ptr<storage::IMetadataStore> metadata,
                     core::config::DurabilityProfile durability)
    : metadata_(std::move(metadata)),
      objects_(storage::StorageLayout(std::move(storage_root)), metadata_, durability) {}

core::Status S3Service::initialize() {
    return objects_.initialize();
}

core::Result<storage::BucketRecord> S3Service::create_bucket(
    const DispatchResult& request) {
    if (request.operation != Operation::CreateBucket) {
        return core::Status::Error(core::StatusCode::FailedPrecondition,
                                   "Request is not a CreateBucket operation");
    }
    if (!metadata_) {
        return core::Status::Error(core::StatusCode::FailedPrecondition,
                                   "Metadata store is not configured");
    }
    if (!request.target.bucket || request.target.key) {
        return core::Status::Error(core::StatusCode::InvalidArgument,
                                   "CreateBucket requires a bucket and no object key");
    }
    return metadata_->create_bucket(*request.target.bucket);
}

core::Result<storage::BucketRecord> S3Service::resolve_bucket(
    const DispatchResult& request, Operation expected_operation, bool require_key) {
    if (request.operation != expected_operation) {
        return core::Status::Error(core::StatusCode::FailedPrecondition,
                                   "Request operation does not match service method");
    }
    if (!metadata_) {
        return core::Status::Error(core::StatusCode::FailedPrecondition,
                                   "Metadata store is not configured");
    }
    if (!request.target.bucket || (require_key && !request.target.key)) {
        return core::Status::Error(core::StatusCode::InvalidArgument,
                                   "Object operation requires a bucket and object key");
    }
    auto bucket = metadata_->find_bucket(*request.target.bucket);
    if (!bucket.ok() && bucket.status().code() == core::StatusCode::NotFound) {
        return core::Status::Error(core::StatusCode::NotFound,
                                   "Bucket does not exist", "bucket");
    }
    return bucket;
}

core::Result<storage::ObjectWriteSession> S3Service::begin_put(
    const DispatchResult& request) {
    auto bucket = resolve_bucket(request, Operation::PutObject, true);
    if (!bucket.ok()) return bucket.status();
    return objects_.open_writer(bucket.value().id, *request.target.key);
}

core::Result<storage::ObjectStore::ObjectReadSession> S3Service::open_get(
    const DispatchResult& request) {
    auto bucket = resolve_bucket(request, Operation::GetObject, true);
    if (!bucket.ok()) return bucket.status();
    return objects_.open_reader(bucket.value().id, *request.target.key);
}

core::Status S3Service::delete_object(const DispatchResult& request) {
    auto bucket = resolve_bucket(request, Operation::DeleteObject, true);
    if (!bucket.ok()) return bucket.status();
    auto status = objects_.delete_object(bucket.value().id, *request.target.key);
    if (status.code() == core::StatusCode::NotFound) return core::Status::Ok();
    return status;
}

} // namespace localhive::s3