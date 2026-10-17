#ifndef LOCALHIVE_CORE_LIFECYCLE_HPP
#define LOCALHIVE_CORE_LIFECYCLE_HPP

#include "localhive/core/types.hpp"
#include <atomic>
#include <functional>
#include <vector>
#include <mutex>
#include <chrono>
#include <memory>
#include <string>

namespace localhive::core::lifecycle {

class CancellationToken {
public:
    CancellationToken() = default;

    [[nodiscard]] bool is_cancelled() const noexcept {
        return cancelled_.load(std::memory_order_acquire);
    }

    void cancel() noexcept;
    void register_callback(std::function<void()> cb);

private:
    std::atomic_bool cancelled_{false};
    std::vector<std::function<void()>> callbacks_;
    std::mutex mutex_;
};

class IService {
public:
    virtual ~IService() = default;

    // Transition from Initialized to Running. Must be non-blocking.
    virtual Status start() = 0;

    // Request the service to initiate orderly shutdown.
    virtual void request_stop() = 0;

    // Block until all internal background routines stop or timeout expires.
    virtual Status wait_for_stop(std::chrono::milliseconds timeout) = 0;
};

// Component Registry: shuts down registered components in strict REVERSE order of registration
class ComponentRegistry {
public:
    ComponentRegistry() = default;
    ~ComponentRegistry();

    void register_component(std::string name, std::shared_ptr<IService> component);
    Status start_all();
    Status shutdown_all(std::chrono::milliseconds per_component_timeout);

    [[nodiscard]] size_t size() const;

private:
    struct Entry {
        std::string name;
        std::shared_ptr<IService> component;
        bool started{false};
    };
    std::vector<Entry> components_;
    mutable std::mutex mutex_;
};

// Coordinates ordered startup and reverse-order shutdown for a service graph.
// Instances are single-use: after shutdown, construct a new runner to restart.
class ServiceRunner {
public:
    explicit ServiceRunner(ComponentRegistry& components,
                           std::chrono::milliseconds drain_timeout =
                               std::chrono::milliseconds(30000))
        : components_(components), drain_timeout_(drain_timeout) {}

    Status start();
    Status request_stop();
    [[nodiscard]] bool is_running() const noexcept {
        return running_.load(std::memory_order_acquire);
    }

private:
    ComponentRegistry& components_;
    std::chrono::milliseconds drain_timeout_;
    std::atomic_bool running_{false};
    std::atomic_bool stopped_{false};
    std::mutex mutex_;
    Status stop_status_{Status::Ok()};
};

class SignalHandler {
public:
    using Callback = std::function<void()>;

    static void install(Callback on_shutdown_signal);
    static void uninstall();
    static void trigger_manual_signal_for_testing();

private:
    static std::mutex mutex_;
    static Callback callback_;
    static bool installed_;
};

} // namespace localhive::core::lifecycle

#endif // LOCALHIVE_CORE_LIFECYCLE_HPP
