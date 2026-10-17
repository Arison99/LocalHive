#include "localhive/core/lifecycle/lifecycle.hpp"
#include <iostream>
#include <cassert>
#include <atomic>
#include <vector>

using namespace localhive::core;
using namespace localhive::core::lifecycle;

void test_cancellation_token() {
    std::cout << "[Test] CancellationToken..." << std::endl;
    CancellationToken token;
    assert(!token.is_cancelled());

    std::atomic_int counter{0};
    token.register_callback([&counter]() {
        counter.fetch_add(1);
    });
    token.register_callback([&counter]() {
        counter.fetch_add(10);
    });

    assert(counter.load() == 0);
    token.cancel();
    assert(token.is_cancelled());
    assert(counter.load() == 11);

    // Register callback AFTER cancellation triggers immediately
    token.register_callback([&counter]() {
        counter.fetch_add(100);
    });
    assert(counter.load() == 111);

    std::cout << "  ✓ CancellationToken and callbacks verified." << std::endl;
}

class MockService : public IService {
public:
    explicit MockService(int id, std::vector<int>& stop_order)
        : id_(id), stop_order_(stop_order) {}

    Status start() override {
        started_ = true;
        return Status::Ok();
    }

    void request_stop() override {
        stopped_ = true;
        stop_order_.push_back(id_);
    }

    Status wait_for_stop(std::chrono::milliseconds /*timeout*/) override {
        return Status::Ok();
    }

    bool is_started() const { return started_; }
    bool is_stopped() const { return stopped_; }

private:
    int id_;
    std::vector<int>& stop_order_;
    bool started_{false};
    bool stopped_{false};
};

void test_component_registry_reverse_shutdown() {
    std::cout << "[Test] ComponentRegistry reverse shutdown..." << std::endl;
    ComponentRegistry registry;
    std::vector<int> stop_order;

    auto s1 = std::make_shared<MockService>(1, stop_order);
    auto s2 = std::make_shared<MockService>(2, stop_order);
    auto s3 = std::make_shared<MockService>(3, stop_order);

    registry.register_component("Service1", s1);
    registry.register_component("Service2", s2);
    registry.register_component("Service3", s3);

    assert(registry.size() == 3);

    auto status = registry.shutdown_all(std::chrono::milliseconds(1000));
    assert(status.ok());

    // Expected shutdown order: 3, then 2, then 1 (reverse order)
    assert(stop_order.size() == 3);
    assert(stop_order[0] == 3);
    assert(stop_order[1] == 2);
    assert(stop_order[2] == 1);

    std::cout << "  ✓ Components stopped in strict reverse order of registration." << std::endl;
}

int main() {
    std::cout << "=== Running Lifecycle Tests ===" << std::endl;
    test_cancellation_token();
    test_component_registry_reverse_shutdown();
    std::cout << "=== All Lifecycle Tests Passed! ===\n" << std::endl;
    return 0;
}
