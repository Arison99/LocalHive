#ifndef LOCALHIVE_CORE_CONCURRENCY_BOUNDED_THREAD_POOL_HPP
#define LOCALHIVE_CORE_CONCURRENCY_BOUNDED_THREAD_POOL_HPP

#include "localhive/core/types.hpp"
#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <deque>

namespace localhive::core::concurrency {

// A fixed-size worker pool with a strictly bounded pending-task queue.
// submit() never blocks: callers can translate ResourceExhausted into
// protocol-level backpressure (for example, S3 SlowDown).
class BoundedThreadPool {
public:
    BoundedThreadPool(size_t worker_count, size_t max_pending_tasks,
                      size_t high_watermark_pct = 80)
        : max_pending_tasks_(max_pending_tasks),
          high_watermark_pct_(high_watermark_pct) {
        if (worker_count == 0 || max_pending_tasks == 0 ||
            high_watermark_pct == 0 || high_watermark_pct >= 100) {
            construction_status_ = Status::Error(
                StatusCode::InvalidArgument,
                "Worker count and queue capacity must be > 0; watermark must be 1-99");
            return;
        }
        workers_.reserve(worker_count);
        try {
            for (size_t i = 0; i < worker_count; ++i) {
                workers_.emplace_back([this] { worker_loop(); });
            }
        } catch (...) {
            construction_status_ = Status::Error(
                StatusCode::ResourceExhausted, "Unable to create worker threads");
            stop(false);
        }
    }

    BoundedThreadPool(const BoundedThreadPool&) = delete;
    BoundedThreadPool& operator=(const BoundedThreadPool&) = delete;

    ~BoundedThreadPool() { stop(true); }

    [[nodiscard]] Status status() const { return construction_status_; }

    [[nodiscard]] Status submit(std::function<void()> task) {
        if (!task) {
            return Status::Error(StatusCode::InvalidArgument, "Task cannot be empty");
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!construction_status_.ok()) return construction_status_;
            if (stopping_) {
                return Status::Error(StatusCode::FailedPrecondition,
                                     "Thread pool is stopping");
            }
            if (tasks_.size() >= max_pending_tasks_) {
                return Status::Error(StatusCode::ResourceExhausted,
                                     "Thread pool task queue is full");
            }
            tasks_.push_back(std::move(task));
        }
        ready_.notify_one();
        return Status::Ok();
    }

    // True at or above the configured queue watermark.
    [[nodiscard]] bool above_high_watermark() const {
        std::lock_guard<std::mutex> lock(mutex_);
        const size_t threshold = (max_pending_tasks_ / 100) * high_watermark_pct_ +
            ((max_pending_tasks_ % 100) * high_watermark_pct_ + 99) / 100;
        return tasks_.size() >= threshold;
    }

    [[nodiscard]] size_t pending_tasks() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size();
    }

    // drain=true completes queued work; drain=false discards pending work.
    void stop(bool drain = true) noexcept {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) return;
            stopping_ = true;
            drain_ = drain;
            if (!drain_) tasks_.clear();
        }
        ready_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
        workers_.clear();
    }

private:
    void worker_loop() noexcept {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                ready_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
                if (stopping_ && (!drain_ || tasks_.empty())) return;
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }
            try { task(); } catch (...) { /* Task failures stay within the task boundary. */ }
        }
    }

    const size_t max_pending_tasks_;
    const size_t high_watermark_pct_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
    Status construction_status_{Status::Ok()};
    bool stopping_{false};
    bool drain_{true};
};

} // namespace localhive::core::concurrency

#endif // LOCALHIVE_CORE_CONCURRENCY_BOUNDED_THREAD_POOL_HPP
