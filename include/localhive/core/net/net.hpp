#ifndef LOCALHIVE_CORE_NET_HPP
#define LOCALHIVE_CORE_NET_HPP

#include "localhive/core/types.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <memory>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>

#if defined(LOCALHIVE_HAS_BOOST_ASIO)
    #include <boost/asio.hpp>
#endif

namespace localhive::core::net {

// 1. Bounded Worker Queue (Governs concurrency and enforces backpressure)
struct WorkerQueueConfig {
    size_t worker_threads{4};
    size_t max_queue_depth{1024};
    size_t high_watermark_pct{80};
};

class BoundedWorkerQueue {
public:
    using Task = std::function<void()>;

    explicit BoundedWorkerQueue(WorkerQueueConfig config);
    ~BoundedWorkerQueue();

    // Submits task. Returns ResourceExhausted if queue depth >= max_queue_depth
    Status submit(Task task);

    [[nodiscard]] size_t current_queue_depth() const noexcept;
    [[nodiscard]] bool is_congested() const noexcept;
    [[nodiscard]] bool is_running() const noexcept;

    void shutdown();

private:
    void worker_loop();

    WorkerQueueConfig config_;
    std::atomic_bool running_{true};
    std::queue<Task> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::thread> workers_;
};

// 2. Network Service Wrapper & Context
class NetContext {
public:
    explicit NetContext(size_t thread_pool_size = 1);
    ~NetContext();

    Status start();
    void stop();
    [[nodiscard]] bool is_running() const noexcept;

#if defined(LOCALHIVE_HAS_BOOST_ASIO)
    // Returns the first context for compatibility with single-loop callers.
    boost::asio::io_context& asio_context() noexcept;
    [[nodiscard]] boost::asio::io_context& asio_context(size_t index) noexcept;
    // Round-robin selection for assigning newly accepted connections.
    [[nodiscard]] boost::asio::io_context& next_asio_context() noexcept;
#endif

private:
    size_t thread_count_;
    std::atomic_bool running_{false};
    std::vector<std::thread> threads_;

#if defined(LOCALHIVE_HAS_BOOST_ASIO)
    struct Loop {
        using Guard = boost::asio::executor_work_guard<
            boost::asio::io_context::executor_type>;
        boost::asio::io_context context;
        std::unique_ptr<Guard> work_guard;
    };
    std::vector<std::unique_ptr<Loop>> loops_;
    std::atomic_size_t next_loop_{0};
#endif
};

// 3. Socket Options Helper
Status set_socket_tcp_nodelay(int socket_fd, bool enable);
Status set_socket_keepalive(int socket_fd, bool enable);

} // namespace localhive::core::net

#endif // LOCALHIVE_CORE_NET_HPP
