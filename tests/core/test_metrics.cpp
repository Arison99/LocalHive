#include "localhive/core/metrics/metrics.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>

using namespace localhive::core::metrics;

void test_counter_and_gauge_basics() {
    std::cout << "[Test] Counter and Gauge basics..." << std::endl;
    Counter c;
    assert(c.get() == 0);
    c.increment();
    assert(c.get() == 1);
    c.increment(10);
    assert(c.get() == 11);
    c.reset();
    assert(c.get() == 0);

    Gauge g(50);
    assert(g.get() == 50);
    g.increment(10);
    assert(g.get() == 60);
    g.decrement(25);
    assert(g.get() == 35);
    g.set(-10);
    assert(g.get() == -10);

    std::cout << "  ✓ Counter and Gauge operations verified." << std::endl;
}

void test_concurrent_metric_updates() {
    std::cout << "[Test] Concurrent metric updates..." << std::endl;
    auto& reg = MetricsRegistry::instance();
    reg.clear();

    auto req_counter = reg.counter("http_requests_total");
    auto inflight_gauge = reg.gauge("active_connections");

    constexpr int kThreads = 8;
    constexpr int kOpsPerThread = 1000;

    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&req_counter, &inflight_gauge]() {
            for (int i = 0; i < kOpsPerThread; ++i) {
                req_counter->increment();
                inflight_gauge->increment();
                inflight_gauge->decrement();
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    assert(req_counter->get() == kThreads * kOpsPerThread);
    assert(inflight_gauge->get() == 0);

    auto snap = reg.snapshot();
    assert(snap.size() == 2);
    std::cout << "  ✓ Concurrent updates completed: " << req_counter->get() << " counts without race." << std::endl;
}

int main() {
    std::cout << "=== Running Metrics Tests ===" << std::endl;
    test_counter_and_gauge_basics();
    test_concurrent_metric_updates();
    std::cout << "=== All Metrics Tests Passed! ===\n" << std::endl;
    return 0;
}
