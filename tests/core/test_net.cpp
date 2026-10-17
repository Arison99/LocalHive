#include "localhive/core/net/net.hpp"
#include <iostream>
#include <cassert>
#include <atomic>
#include <chrono>

using namespace localhive::core;
using namespace localhive::core::net;

void test_bounded_worker_queue_execution() {
    std::cout << "[Test] BoundedWorkerQueue task execution..." << std::endl;
    WorkerQueueConfig cfg;
    cfg.worker_threads = 4;
    cfg.max_queue_depth = 100;
    cfg.high_watermark_pct = 80;

    BoundedWorkerQueue queue(cfg);
    assert(queue.is_running());

    std::atomic<int> completed{0};
    constexpr int kTotalTasks = 50;

    for (int i = 0; i < kTotalTasks; ++i) {
        auto st = queue.submit([&completed]() {
            completed.fetch_add(1);
        });
        assert(st.ok());
    }

    // Wait for completion
    for (int retry = 0; retry < 50 && completed.load() < kTotalTasks; ++retry) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    assert(completed.load() == kTotalTasks);
    std::cout << "  ✓ All " << kTotalTasks << " tasks completed successfully." << std::endl;
}

void test_worker_queue_capacity_rejection() {
    std::cout << "[Test] Worker queue capacity rejection..." << std::endl;
    WorkerQueueConfig cfg;
    cfg.worker_threads = 1; // Single worker to allow queue buildup
    cfg.max_queue_depth = 5;
    cfg.high_watermark_pct = 60;

    BoundedWorkerQueue queue(cfg);

    // Block the worker thread temporarily
    std::atomic<bool> unblock{false};
    std::atomic<bool> worker_started{false};
    auto st0 = queue.submit([&unblock, &worker_started]() {
        worker_started.store(true);
        while (!unblock.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    assert(st0.ok());

    // Wait until worker task is actively running and popped from queue
    while (!worker_started.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Fill queue to capacity (max_queue_depth = 5)
    for (int i = 0; i < 5; ++i) {
        auto st = queue.submit([]() {});
        assert(st.ok());
    }

    // Congestion should be active
    assert(queue.is_congested());

    // Next submit must be rejected with ResourceExhausted
    auto reject_st = queue.submit([]() {});
    assert(!reject_st.ok());
    assert(reject_st.code() == StatusCode::ResourceExhausted);

    // Unblock worker and let queue drain
    unblock.store(true);
    queue.shutdown();
    assert(!queue.is_running());

    std::cout << "  ✓ Queue capacity exceeded rejected with ResourceExhausted." << std::endl;
}

void test_net_context_lifecycle() {
    std::cout << "[Test] NetContext lifecycle..." << std::endl;
    NetContext ctx(2);
    assert(!ctx.is_running());

    auto st = ctx.start();
    assert(st.ok());
    assert(ctx.is_running());

    ctx.stop();
    assert(!ctx.is_running());

    std::cout << "  ✓ NetContext start and stop lifecycle verified." << std::endl;
}

int main() {
    std::cout << "=== Running Network Core Tests ===" << std::endl;
    test_bounded_worker_queue_execution();
    test_worker_queue_capacity_rejection();
    test_net_context_lifecycle();
    std::cout << "=== All Network Core Tests Passed! ===\n" << std::endl;
    return 0;
}
