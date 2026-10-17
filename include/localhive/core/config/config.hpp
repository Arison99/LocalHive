#ifndef LOCALHIVE_CORE_CONFIG_HPP
#define LOCALHIVE_CORE_CONFIG_HPP

#include "localhive/core/types.hpp"
#include <string>
#include <cstdint>
#include <chrono>
#include <vector>

namespace localhive::core::config {

enum class DurabilityProfile {
    Strict,
    Standard,
    Development
};

[[nodiscard]] std::string_view durability_profile_to_string(DurabilityProfile profile) noexcept;
[[nodiscard]] Result<DurabilityProfile> parse_durability_profile(std::string_view str) noexcept;

struct ServerConfig {
    std::string host{"0.0.0.0"};
    uint16_t port{8080};
    uint32_t thread_pool_size{4};
    std::chrono::milliseconds request_timeout_ms{30000};
};

struct StorageConfig {
    std::string storage_root{"buckets"};
    DurabilityProfile durability{DurabilityProfile::Standard};
    uint64_t max_staging_bytes{20ULL * 1024 * 1024 * 1024}; // 20 GB
    uint32_t staging_grace_period_sec{3600};
    bool wal_enabled{true};
};

struct ProxyConfig {
    std::string listen_host{"0.0.0.0"};
    uint16_t listen_port{3306};
    std::string upstream_host{"127.0.0.1"};
    uint16_t upstream_port{3306};
    uint32_t max_connections{1024};
    uint32_t prewarm_pool_size{10};
    std::chrono::milliseconds connect_timeout_ms{5000};
    std::chrono::milliseconds idle_timeout_ms{300000};
};

struct ConcurrencyConfig {
    uint32_t max_active_connections{1024};
    uint32_t max_concurrent_uploads{128};
    size_t max_header_bytes{16 * 1024};
    uint64_t max_single_put_body_bytes{5ULL * 1024 * 1024 * 1024};
    std::chrono::milliseconds header_read_timeout_ms{10000};
    std::chrono::milliseconds body_idle_timeout_ms{30000};
    size_t per_connection_read_buffer_bytes{64 * 1024};
    size_t max_global_queued_payload_bytes{64 * 1024 * 1024}; // 64 MB
    uint32_t storage_worker_threads{8};
    size_t storage_queue_max_tasks{1024};
    size_t storage_queue_high_watermark_pct{80};
    size_t sqlite_write_queue_max_tasks{512};
    uint32_t max_open_file_descriptors{2048};
    uint32_t max_active_reader_leases{1024};
    size_t staging_disk_high_watermark_pct{85};
};

struct AppConfig {
    ServerConfig server;
    StorageConfig storage;
    ProxyConfig proxy;
    ConcurrencyConfig concurrency;
};

// Validations
[[nodiscard]] Status validate_server_config(const ServerConfig& config);
[[nodiscard]] Status validate_storage_config(const StorageConfig& config);
[[nodiscard]] Status validate_proxy_config(const ProxyConfig& config);
[[nodiscard]] Status validate_concurrency_config(const ConcurrencyConfig& config);
[[nodiscard]] Status validate_app_config(const AppConfig& config);

// Command-line and Environment Variable parser
class ConfigParser {
public:
    static Result<AppConfig> parse_cli_and_env(
        int argc, const char* const* argv,
        const AppConfig& defaults = AppConfig());

    static Result<AppConfig> parse_args(
        const std::vector<std::string>& args,
        const AppConfig& defaults = AppConfig());
};

} // namespace localhive::core::config

#endif // LOCALHIVE_CORE_CONFIG_HPP
