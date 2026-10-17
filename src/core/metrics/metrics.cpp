#include "localhive/core/metrics/metrics.hpp"

namespace localhive::core::metrics {

MetricsRegistry& MetricsRegistry::instance() {
    static MetricsRegistry reg;
    return reg;
}

std::shared_ptr<Counter> MetricsRegistry::counter(std::string_view name) {
    std::string key(name);
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = counters_.find(key);
    if (it != counters_.end()) {
        return it->second;
    }
    auto c = std::make_shared<Counter>();
    counters_[key] = c;
    return c;
}

std::shared_ptr<Gauge> MetricsRegistry::gauge(std::string_view name) {
    std::string key(name);
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = gauges_.find(key);
    if (it != gauges_.end()) {
        return it->second;
    }
    auto g = std::make_shared<Gauge>();
    gauges_[key] = g;
    return g;
}

std::vector<MetricSnapshot> MetricsRegistry::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MetricSnapshot> result;
    result.reserve(counters_.size() + gauges_.size());

    for (const auto& [name, c] : counters_) {
        if (c) {
            result.push_back({name, "counter", static_cast<int64_t>(c->get())});
        }
    }
    for (const auto& [name, g] : gauges_) {
        if (g) {
            result.push_back({name, "gauge", g->get()});
        }
    }
    return result;
}

void MetricsRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    counters_.clear();
    gauges_.clear();
}

} // namespace localhive::core::metrics
