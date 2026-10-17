#include "localhive/core/config/config.hpp"
#include <iostream>
#include <cassert>

using namespace localhive::core;
using namespace localhive::core::config;

void test_default_config_validation() {
    std::cout << "[Test] Default config validation..." << std::endl;
    AppConfig defaults;
    auto st = validate_app_config(defaults);
    assert(st.ok());
    assert(defaults.server.port == 8080);
    assert(defaults.storage.durability == DurabilityProfile::Standard);
    std::cout << "  ✓ Default configuration is valid." << std::endl;
}

void test_invalid_config_rejection() {
    std::cout << "[Test] Invalid config rejection..." << std::endl;
    ServerConfig bad_server;
    bad_server.port = 0; // Port 0 is invalid
    auto s1 = validate_server_config(bad_server);
    assert(!s1.ok());
    assert(s1.code() == StatusCode::InvalidArgument);

    StorageConfig bad_storage;
    bad_storage.storage_root = ""; // Empty path is invalid
    auto s2 = validate_storage_config(bad_storage);
    assert(!s2.ok());

    ProxyConfig bad_proxy;
    bad_proxy.upstream_host = "";
    auto s3 = validate_proxy_config(bad_proxy);
    assert(!s3.ok());

    std::cout << "  ✓ Invalid configurations rejected with actionable errors." << std::endl;
}

void test_cli_parsing() {
    std::cout << "[Test] Command-line parsing..." << std::endl;
    std::vector<std::string> args = {
        "--port", "9000",
        "--storage-root", "/data/my-storage",
        "--durability", "strict",
        "--proxy-port", "3307",
        "--upstream-port", "5432"
    };

    auto res = ConfigParser::parse_args(args);
    assert(res.ok());
    const auto& cfg = res.value();
    assert(cfg.server.port == 9000);
    assert(cfg.storage.storage_root == "/data/my-storage");
    assert(cfg.storage.durability == DurabilityProfile::Strict);
    assert(cfg.proxy.listen_port == 3307);
    assert(cfg.proxy.upstream_port == 5432);

    std::cout << "  ✓ Command-line flags parsed and validated." << std::endl;
}

void test_unknown_cli_flag_rejection() {
    std::cout << "[Test] Unknown CLI flag rejection..." << std::endl;
    std::vector<std::string> args = {"--invalid-flag", "123"};
    auto res = ConfigParser::parse_args(args);
    assert(!res.ok());
    assert(res.status().code() == StatusCode::InvalidArgument);
    std::cout << "  ✓ Unknown flags rejected." << std::endl;
}

int main() {
    std::cout << "=== Running Config Tests ===" << std::endl;
    test_default_config_validation();
    test_invalid_config_rejection();
    test_cli_parsing();
    test_unknown_cli_flag_rejection();
    std::cout << "=== All Config Tests Passed! ===\n" << std::endl;
    return 0;
}
