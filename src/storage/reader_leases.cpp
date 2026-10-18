#include "localhive/storage/reader_leases.hpp"

namespace localhive::storage {

ReaderLease::~ReaderLease() { release(); }

ReaderLease::ReaderLease(ReaderLease&& other) noexcept
    : manager_(other.manager_), generation_id_(other.generation_id_) {
    other.manager_ = nullptr;
}

ReaderLease& ReaderLease::operator=(ReaderLease&& other) noexcept {
    if (this == &other) return *this;
    release();
    manager_ = other.manager_;
    generation_id_ = other.generation_id_;
    other.manager_ = nullptr;
    return *this;
}

void ReaderLease::release() noexcept {
    if (manager_ != nullptr) manager_->release(generation_id_);
    manager_ = nullptr;
}

ReaderLease ReaderLeaseManager::acquire(GenerationId generation_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++lease_counts_[generation_id];
    return ReaderLease(this, generation_id);
}

size_t ReaderLeaseManager::active_leases(GenerationId generation_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = lease_counts_.find(generation_id);
    return found == lease_counts_.end() ? 0 : found->second;
}

core::Result<bool> ReaderLeaseManager::reclaim_if_unleased(
    GenerationId generation_id, const std::function<core::Status()>& reclaim) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (lease_counts_.find(generation_id) != lease_counts_.end()) return false;
    if (!reclaim) return core::Status::Error(core::StatusCode::InvalidArgument,
                                             "Reclamation callback cannot be empty");
    const auto status = reclaim();
    if (!status.ok()) return status;
    return true;
}

void ReaderLeaseManager::release(GenerationId generation_id) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = lease_counts_.find(generation_id);
    if (found == lease_counts_.end()) return;
    if (--found->second == 0) lease_counts_.erase(found);
}

} // namespace localhive::storage
