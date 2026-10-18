#include "localhive/storage/garbage_collector.hpp"

#include "localhive/core/platform/platform.hpp"

namespace localhive::storage {

core::Result<GarbageCollectionReport> GarbageCollector::collect() {
    auto candidates = metadata_.reclaimable_generations();
    if (!candidates.ok()) return candidates.status();
    GarbageCollectionReport report;
    for (const auto& candidate : candidates.value()) {
        const auto reclaimed = reader_leases_.reclaim_if_unleased(
            candidate.identity.generation_id, [this, &candidate]() {
                auto status = core::platform::safe_delete_file(
                    layout_.object_path(candidate.identity));
                if (!status.ok()) return status;
                return metadata_.remove_generation(candidate.identity.generation_id);
            });
        if (!reclaimed.ok()) return reclaimed.status();
        if (reclaimed.value()) ++report.reclaimed;
        else ++report.retained_by_reader;
    }
    return report;
}

} // namespace localhive::storage
