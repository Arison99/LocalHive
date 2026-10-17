#include "localhive/core/net/net.hpp"

#if defined(_WIN32) || defined(_WIN64)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <unistd.h>
#endif

namespace localhive::core::net {

BoundedWorkerQueue::BoundedWorkerQueue(WorkerQueueConfig config)
    : config_(config) {
    size_t count = config_.worker_threads > 0 ? config_.worker_threads : 1;
    workers_.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        workers_.emplace_back(&BoundedWorkerQueue::worker_loop, this);
    }
}

BoundedWorkerQueue::~BoundedWorkerQueue() {
    shutdown();
}

Status BoundedWorkerQueue::submit(Task task) {
    if (!task) {
        return Status::Error(StatusCode::InvalidArgument, "Cannot submit null task");
    }
    if (!running_.load(std::memory_order_acquire)) {
        return Status::Error(StatusCode::Unavailable, "Worker queue has been shut down");
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.size() >= config_.max_queue_depth) {
            return Status::Error(StatusCode::ResourceExhausted, "Worker queue depth capacity exceeded");
        }
        queue_.push(std::move(task));
    }
    cv_.notify_one();
    return Status::Ok();
}

size_t BoundedWorkerQueue::current_queue_depth() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

bool BoundedWorkerQueue::is_congested() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (config_.max_queue_depth == 0) return false;
    size_t pct = (queue_.size() * 100) / config_.max_queue_depth;
    return pct >= config_.high_watermark_pct;
}

bool BoundedWorkerQueue::is_running() const noexcept {
    return running_.load(std::memory_order_acquire);
}

void BoundedWorkerQueue::shutdown() {
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        return; // Already shut down
    }
    cv_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) {
            w.join();
        }
    }
    workers_.clear();
}

void BoundedWorkerQueue::worker_loop() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() {
                return !running_.load(std::memory_order_acquire) || !queue_.empty();
            });

            if (!running_.load(std::memory_order_acquire) && queue_.empty()) {
                return;
            }

            if (!queue_.empty()) {
                task = std::move(queue_.front());
                queue_.pop();
            }
        }

        if (task) {
            task();
        }
    }
}

#if defined(LOCALHIVE_HAS_BOOST_ASIO)
NetContext::NetContext(size_t thread_pool_size)
    : thread_count_(thread_pool_size > 0 ? thread_pool_size : 1),
      work_guard_(boost::asio::make_work_guard(io_context_)) {}

NetContext::~NetContext() {
    stop();
}

Status NetContext::start() {
    if (running_.exchange(true, std::memory_order_acq_rel)) {
        return Status::Ok(); // Already running
    }
    threads_.reserve(thread_count_);
    for (size_t i = 0; i < thread_count_; ++i) {
        threads_.emplace_back([this]() {
            io_context_.run();
        });
    }
    return Status::Ok();
}

void NetContext::stop() {
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    work_guard_.reset();
    io_context_.stop();
    for (auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    threads_.clear();
}

bool NetContext::is_running() const noexcept {
    return running_.load(std::memory_order_acquire);
}

#else

NetContext::NetContext(size_t thread_pool_size)
    : thread_count_(thread_pool_size > 0 ? thread_pool_size : 1) {}

NetContext::~NetContext() {
    stop();
}

Status NetContext::start() {
    running_.store(true, std::memory_order_release);
    return Status::Ok();
}

void NetContext::stop() {
    running_.store(false, std::memory_order_release);
}

bool NetContext::is_running() const noexcept {
    return running_.load(std::memory_order_acquire);
}
#endif

Status set_socket_tcp_nodelay(int socket_fd, bool enable) {
    if (socket_fd < 0) {
        return Status::Error(StatusCode::InvalidArgument, "Invalid socket descriptor");
    }
    int optval = enable ? 1 : 0;
#if defined(_WIN32) || defined(_WIN64)
    int rc = ::setsockopt(
        static_cast<SOCKET>(socket_fd),
        IPPROTO_TCP,
        TCP_NODELAY,
        reinterpret_cast<const char*>(&optval),
        sizeof(optval));
    if (rc != 0) {
        return Status::Error(StatusCode::Internal, "setsockopt TCP_NODELAY failed", std::to_string(::WSAGetLastError()));
    }
#else
    int rc = ::setsockopt(
        socket_fd,
        IPPROTO_TCP,
        TCP_NODELAY,
        &optval,
        sizeof(optval));
    if (rc != 0) {
        return Status::Error(StatusCode::Internal, "setsockopt TCP_NODELAY failed", std::strerror(errno));
    }
#endif
    return Status::Ok();
}

Status set_socket_keepalive(int socket_fd, bool enable) {
    if (socket_fd < 0) {
        return Status::Error(StatusCode::InvalidArgument, "Invalid socket descriptor");
    }
    int optval = enable ? 1 : 0;
#if defined(_WIN32) || defined(_WIN64)
    int rc = ::setsockopt(
        static_cast<SOCKET>(socket_fd),
        SOL_SOCKET,
        SO_KEEPALIVE,
        reinterpret_cast<const char*>(&optval),
        sizeof(optval));
    if (rc != 0) {
        return Status::Error(StatusCode::Internal, "setsockopt SO_KEEPALIVE failed", std::to_string(::WSAGetLastError()));
    }
#else
    int rc = ::setsockopt(
        socket_fd,
        SOL_SOCKET,
        SO_KEEPALIVE,
        &optval,
        sizeof(optval));
    if (rc != 0) {
        return Status::Error(StatusCode::Internal, "setsockopt SO_KEEPALIVE failed", std::strerror(errno));
    }
#endif
    return Status::Ok();
}

} // namespace localhive::core::net
