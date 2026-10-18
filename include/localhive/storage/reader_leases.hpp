#ifndef LOCALHIVE_STORAGE_READER_LEASES_HPP
#define LOCALHIVE_STORAGE_READER_LEASES_HPP

#include "localhive/storage/storage_layout.hpp"
#include "localhive/core/types.hpp"
#include <cstddef>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <functional>

namespace localhive::storage {

class ReaderLeaseManager;

class ReaderLease {
public:
    ReaderLease() = default;
    ~ReaderLease();
    ReaderLease(ReaderLease&& other) noexcept;
    ReaderLease& operator=(ReaderLease&& other) noexcept;
    ReaderLease(const ReaderLease&) = delete;
    ReaderLease& operator=(const ReaderLease&) = delete;

    [[nodiscard]] GenerationId generation_id() const noexcept { return generation_id_; }

private:
    friend class ReaderLeaseManager;
    ReaderLease(ReaderLeaseManager* manager, GenerationId generation_id)
        : manager_(manager), generation_id_(generation_id) {}
    void release() noexcept;
    ReaderLeaseManager* manager_{nullptr};
    GenerationId generation_id_{};
};

class ReaderLeaseManager {
public:
    [[nodiscard]] ReaderLease acquire(GenerationId generation_id);
    [[nodiscard]] size_t active_leases(GenerationId generation_id) const;
    // Executes reclamation only while no reader holds this generation. Lease
    // acquisition is blocked for the duration of the callback.
    [[nodiscard]] core::Result<bool> reclaim_if_unleased(
        GenerationId generation_id, const std::function<core::Status()>& reclaim);

private:
    friend class ReaderLease;
    void release(GenerationId generation_id) noexcept;
    mutable std::mutex mutex_;
    std::unordered_map<GenerationId, size_t> lease_counts_;
};

} // namespace localhive::storage

#endif // LOCALHIVE_STORAGE_READER_LEASES_HPP
