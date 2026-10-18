#include "localhive/storage/recovery.hpp"

#include "localhive/core/platform/platform.hpp"
#include <system_error>

namespace localhive::storage {

RecoveryScanner::RecoveryScanner(StorageLayout layout, IMetadataStore& metadata,
                                 std::chrono::seconds staging_grace_period)
    : layout_(std::move(layout)), metadata_(metadata),
      staging_grace_period_(staging_grace_period) {}

bool RecoveryScanner::is_staging_expired(const std::filesystem::path& path) const {
    std::error_code error;
    const auto modified = std::filesystem::last_write_time(path, error);
    if (error) return true;
    const auto now = decltype(modified)::clock::now();
    return now - modified >= staging_grace_period_;
}

core::Result<RecoveryReport> RecoveryScanner::run() {
    auto candidates = metadata_.recovery_candidates();
    if (!candidates.ok()) return candidates.status();
    RecoveryReport report;
    for (const auto& candidate : candidates.value()) {
        const auto staging = layout_.staging_path(candidate.identity.bucket_id,
                                                  candidate.identity.generation_id);
        const auto final_path = layout_.object_path(candidate.identity);
        core::Status status;
        std::error_code exists_error;
        switch (candidate.state) {
        case GenerationState::Receiving: {
            // A fresh upload may still be active during a clean restart window.
            // Leave it until the configured grace period has elapsed.
            const bool staging_exists = std::filesystem::exists(staging, exists_error);
            if (exists_error) return core::Status::Error(core::StatusCode::Internal,
                "Could not inspect staging file", exists_error.message());
            if (staging_exists && !is_staging_expired(staging)) {
                ++report.retained_recent_staging;
                break;
            }
            status = core::platform::safe_delete_file(staging);
            if (!status.ok()) return status;
            status = metadata_.remove_generation(candidate.identity.generation_id);
            if (!status.ok()) return status;
            ++report.purged_staging;
            break;
        }
        case GenerationState::Staged: {
            status = core::platform::safe_delete_file(staging);
            if (!status.ok()) return status;
            status = metadata_.remove_generation(candidate.identity.generation_id);
            if (!status.ok()) return status;
            ++report.purged_staging;
            break;
        }
        case GenerationState::Prepared: {
            // Prepared data was moved successfully but was never referenced by
            // metadata, so removing it preserves the previously active object.
            status = core::platform::safe_delete_file(final_path);
            if (!status.ok()) return status;
            status = metadata_.remove_generation(candidate.identity.generation_id);
            if (!status.ok()) return status;
            ++report.purged_prepared;
            break;
        }
        case GenerationState::Published: {
            // SQLite committed first; retain the data and finalize its state
            // only after checking the file survived the restart.
            const bool final_exists = std::filesystem::exists(final_path, exists_error);
            if (exists_error) return core::Status::Error(core::StatusCode::Internal,
                "Could not inspect committed object", exists_error.message());
            if (!final_exists) {
                return core::Status::Error(core::StatusCode::DataLoss,
                    "Published generation is missing its immutable data file",
                    final_path.string());
            }
            status = metadata_.set_generation_state(candidate.identity.generation_id,
                                                    GenerationState::Committed);
            if (!status.ok()) return status;
            ++report.finalized_published;
            break;
        }
        default:
            break;
        }
    }
    return report;
}

} // namespace localhive::storage
