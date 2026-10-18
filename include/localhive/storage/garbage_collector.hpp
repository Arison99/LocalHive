#ifndef LOCALHIVE_STORAGE_GARBAGE_COLLECTOR_HPP
#define LOCALHIVE_STORAGE_GARBAGE_COLLECTOR_HPP

#include "localhive/core/types.hpp"
#include "localhive/storage/metadata_store.hpp"
#include "localhive/storage/reader_leases.hpp"
#include "localhive/storage/storage_layout.hpp"

namespace localhive::storage {

struct GarbageCollectionReport {
    size_t reclaimed{};
    size_t retained_by_reader{};
};

class GarbageCollector {
public:
    GarbageCollector(StorageLayout layout, IMetadataStore& metadata,
                     ReaderLeaseManager& reader_leases)
        : layout_(std::move(layout)), metadata_(metadata), reader_leases_(reader_leases) {}

    [[nodiscard]] core::Result<GarbageCollectionReport> collect();

private:
    StorageLayout layout_;
    IMetadataStore& metadata_;
    ReaderLeaseManager& reader_leases_;
};

} // namespace localhive::storage

#endif // LOCALHIVE_STORAGE_GARBAGE_COLLECTOR_HPP
