#ifndef LOCALHIVE_CORE_METRICS_HPP
#define LOCALHIVE_CORE_METRICS_HPP

#include <string>
#include <string_view>
#include <cstdint>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace localhive::core::metrics {

class Counter {
public:
    explicit Counter(uint64_t initial = 0) noexcept : value_(initial) {}

    void increment(uint64_t delta = 1) noexcept {
        value_.fetch_add(delta, std::memory_order_relaxed);
    }

    [[nodiscard]] uint64_t get() const noexcept {
        return value_.load(std::memory_order_relaxed);
    }

    void reset() noexcept {
        value_.store(0, std::memory_order_relaxed);
    }

private:
    std::atomic<uint64_t> value_;
};

class Gauge {
public:
    explicit Gauge(int64_t initial = 0) noexcept : value_(initial) {}

    void set(int64_t val) noexcept {
        value_.store(val, std::memory_order_relaxed);
    }

    void increment(int64_t delta = 1) noexcept {
        value_.fetch_add(delta, std::memory_order_relaxed);
    }

    void decrement(int64_t delta = 1) noexcept {
        value_.fetch_sub(delta, std::memory_order_relaxed);
    }

    [[nodiscard]] int64_t get() const noexcept {
        return value_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<int64_t> value_;
};

struct MetricSnapshot {
    std::string name;
    std::string type; // "counter" or "gauge"
    int64_t value{0};
};

class MetricsRegistry {
public:
    static MetricsRegistry& instance();

    std::shared_ptr<Counter> counter(std::string_view name);
    std::shared_ptr<Gauge> gauge(std::string_view name);

    [[nodiscard]] std::vector<MetricSnapshot> snapshot() const;
    void clear();

private:
    MetricsRegistry() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Counter>> counters_;
    std::unordered_map<std::string, std::shared_ptr<Gauge>> gauges_;
};

} // namespace localhive::core::metrics

#endif // LOCALHIVE_CORE_METRICS_HPP
