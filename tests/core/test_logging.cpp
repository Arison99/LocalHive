#include "localhive/core/logging/logging.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>

using namespace localhive::core::logging;

class MockSink : public ILogSink {
public:
    void write(const LogRecord& record) override {
        std::lock_guard<std::mutex> lock(m_);
        records.push_back(record);
    }
    void flush() override {}

    std::vector<LogRecord> records;
    std::mutex m_;
};

void test_log_level_filtering() {
    std::cout << "[Test] Log level filtering..." << std::endl;
    auto& logger = Logger::instance();
    auto mock = std::make_shared<MockSink>();
    logger.clear_sinks();
    logger.add_sink(mock);

    logger.set_level(LogLevel::Warn);

    LH_LOG_DEBUG("This debug log should be suppressed");
    LH_LOG_INFO("This info log should be suppressed");
    LH_LOG_WARN("This warning log should pass");
    LH_LOG_ERROR("This error log should pass");

    assert(mock->records.size() == 2);
    assert(mock->records[0].level == LogLevel::Warn);
    assert(mock->records[1].level == LogLevel::Error);

    std::cout << "  ✓ Log levels properly filtered." << std::endl;
}

void test_sensitive_data_redaction() {
    std::cout << "[Test] Sensitive data redaction..." << std::endl;
    std::string sensitive = "Connecting with password=SuperSecretPassword123 and secret=TopSecretKey!";
    std::string redacted = Logger::redact(sensitive);

    assert(redacted.find("SuperSecretPassword123") == std::string::npos);
    assert(redacted.find("TopSecretKey") == std::string::npos);
    assert(redacted.find("[REDACTED]") != std::string::npos);

    std::cout << "  ✓ Sensitive credentials masked: " << redacted << std::endl;
}

void test_concurrent_logging() {
    std::cout << "[Test] Concurrent logging..." << std::endl;
    auto& logger = Logger::instance();
    auto mock = std::make_shared<MockSink>();
    logger.clear_sinks();
    logger.add_sink(mock);
    logger.set_level(LogLevel::Info);

    constexpr int kThreads = 8;
    constexpr int kLogsPerThread = 50;

    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([t]() {
            for (int i = 0; i < kLogsPerThread; ++i) {
                LH_LOG_INFO("Worker thread log message " + std::to_string(t) + ":" + std::to_string(i));
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    assert(mock->records.size() == kThreads * kLogsPerThread);
    std::cout << "  ✓ Concurrent logging recorded " << mock->records.size() << " entries without contention." << std::endl;
}

int main() {
    std::cout << "=== Running Logging Tests ===" << std::endl;
    test_log_level_filtering();
    test_sensitive_data_redaction();
    test_concurrent_logging();
    std::cout << "=== All Logging Tests Passed! ===\n" << std::endl;
    return 0;
}
