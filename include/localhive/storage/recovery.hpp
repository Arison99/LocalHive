#ifndef LOCALHIVE_STORAGE_RECOVERY_HPP
#define LOCALHIVE_STORAGE_RECOVERY_HPP

#include "localhive/core/types.hpp"
#include "localhive/storage/metadata_store.hpp"
#include "localhive/storage/storage_layout.hpp"
#include <chrono>

namespace localhive::storage {

struct RecoveryReport {
    size_t purged_staging{};
    size_t purged_prepared{};
    size_t finalized_published{};
    size_t retained_recent_staging{};
};

class RecoveryScanner {
public:
    RecoveryScanner(StorageLayout layout, IMetadataStore& metadata,
                    std::chrono::seconds staging_grace_period);

    [[nodiscard]] core::Result<RecoveryReport> run();

private:
    [[nodiscard]] bool is_staging_expired(const std::filesystem::path& path) const;
    StorageLayout layout_;
    IMetadataStore& metadata_;
    std::chrono::seconds staging_grace_period_;
};

} // namespace localhive::storage

#endif // LOCALHIVE_STORAGE_RECOVERY_HPP
