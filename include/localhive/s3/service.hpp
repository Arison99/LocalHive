#ifndef LOCALHIVE_S3_SERVICE_HPP
#define LOCALHIVE_S3_SERVICE_HPP

#include "localhive/core/config/config.hpp"
#include "localhive/core/types.hpp"
#include "localhive/s3/request_dispatch.hpp"
#include "localhive/storage/object_store.hpp"
#include <filesystem>
#include <memory>
#include <string>

namespace localhive::s3 {

class S3Service {
public:
    S3Service(std::filesystem::path storage_root,
              std::shared_ptr<storage::IMetadataStore> metadata,
              core::config::DurabilityProfile durability = core::config::DurabilityProfile::Standard);

    [[nodiscard]] core::Status initialize();
    [[nodiscard]] core::Result<storage::BucketRecord> create_bucket(
        const DispatchResult& request);
    [[nodiscard]] core::Result<storage::ObjectWriteSession> begin_put(
        const DispatchResult& request);
    [[nodiscard]] core::Result<storage::ObjectStore::ObjectReadSession> open_get(
        const DispatchResult& request);
    [[nodiscard]] core::Status delete_object(const DispatchResult& request);

private:
    [[nodiscard]] core::Result<storage::BucketRecord> resolve_bucket(
        const DispatchResult& request, Operation expected_operation,
        bool require_key);

    std::shared_ptr<storage::IMetadataStore> metadata_;
    storage::ObjectStore objects_;
};

} // namespace localhive::s3

#endif // LOCALHIVE_S3_SERVICE_HPP