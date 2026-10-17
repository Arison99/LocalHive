#include "localhive/core/config/config.hpp"
#include <charconv>
#include <cstdlib>

namespace localhive::core::config {

std::string_view
durability_profile_to_string(DurabilityProfile profile) noexcept {
  switch (profile) {
  case DurabilityProfile::Strict:
    return "strict";
  case DurabilityProfile::Standard:
    return "standard";
  case DurabilityProfile::Development:
    return "development";
  }
  return "unknown";
}

Result<DurabilityProfile>
parse_durability_profile(std::string_view str) noexcept {
  if (str == "strict" || str == "STRICT") {
    return DurabilityProfile::Strict;
  }
  if (str == "standard" || str == "STANDARD") {
    return DurabilityProfile::Standard;
  }
  if (str == "development" || str == "dev" || str == "DEV") {
    return DurabilityProfile::Development;
  }
  return Status::Error(StatusCode::InvalidArgument,
                       "Unknown durability profile: " + std::string(str));
}

Status validate_server_config(const ServerConfig &config) {
  if (config.port == 0) {
    return Status::Error(StatusCode::InvalidArgument,
                         "Server port cannot be 0");
  }
  if (config.host.empty()) {
    return Status::Error(StatusCode::InvalidArgument,
                         "Server host cannot be empty");
  }
  if (config.thread_pool_size == 0) {
    return Status::Error(StatusCode::InvalidArgument,
                         "Thread pool size must be >= 1");
  }
  return Status::Ok();
}

Status validate_storage_config(const StorageConfig &config) {
  if (config.storage_root.empty()) {
    return Status::Error(StatusCode::InvalidArgument,
                         "Storage root directory cannot be empty");
  }
  if (config.max_staging_bytes == 0) {
    return Status::Error(StatusCode::InvalidArgument,
                         "Max staging bytes must be > 0");
  }
  return Status::Ok();
}

Status validate_proxy_config(const ProxyConfig &config) {
  if (config.listen_port == 0) {
    return Status::Error(StatusCode::InvalidArgument,
                         "Proxy listen port cannot be 0");
  }
  if (config.upstream_port == 0) {
    return Status::Error(StatusCode::InvalidArgument,
                         "Proxy upstream port cannot be 0");
  }
  if (config.upstream_host.empty()) {
    return Status::Error(StatusCode::InvalidArgument,
                         "Proxy upstream host cannot be empty");
  }
  return Status::Ok();
}

Status validate_app_config(const AppConfig &config) {
  auto s1 = validate_server_config(config.server);
  if (!s1.ok())
    return s1;
  auto s2 = validate_storage_config(config.storage);
  if (!s2.ok())
    return s2;
  auto s3 = validate_proxy_config(config.proxy);
  if (!s3.ok())
    return s3;
  return Status::Ok();
}

Result<AppConfig> ConfigParser::parse_cli_and_env(int argc,
                                                  const char *const *argv,
                                                  const AppConfig &defaults) {
  std::vector<std::string> args;
  args.reserve(argc > 0 ? argc - 1 : 0);
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
  return parse_args(args, defaults);
}

Result<AppConfig> ConfigParser::parse_args(const std::vector<std::string> &args,
                                           const AppConfig &defaults) {
  AppConfig cfg = defaults;

  // 1. Process environment variables first
  if (const char *env_port = std::getenv("LOCALHIVE_PORT")) {
    uint32_t val = 0;
    auto [ptr, ec] = std::from_chars(
        env_port, env_port + std::char_traits<char>::length(env_port), val);
    if (ec != std::errc() || val == 0 || val > 65535) {
      return Status::Error(StatusCode::InvalidArgument,
                           "Invalid LOCALHIVE_PORT environment value: " +
                               std::string(env_port));
    }
    cfg.server.port = static_cast<uint16_t>(val);
  }
  if (const char *env_root = std::getenv("LOCALHIVE_STORAGE_ROOT")) {
    if (*env_root != '\0') {
      cfg.storage.storage_root = env_root;
    }
  }
  if (const char *env_dur = std::getenv("LOCALHIVE_DURABILITY")) {
    auto dur_res = parse_durability_profile(env_dur);
    if (!dur_res.ok())
      return dur_res.status();
    cfg.storage.durability = dur_res.value();
  }
  if (const char *env_uport = std::getenv("LOCALHIVE_UPSTREAM_PORT")) {
    uint32_t val = 0;
    auto [ptr, ec] = std::from_chars(
        env_uport, env_uport + std::char_traits<char>::length(env_uport), val);
    if (ec != std::errc() || val == 0 || val > 65535) {
      return Status::Error(
          StatusCode::InvalidArgument,
          "Invalid LOCALHIVE_UPSTREAM_PORT environment value: " +
              std::string(env_uport));
    }
    cfg.proxy.upstream_port = static_cast<uint16_t>(val);
  }

  // 2. Process command line arguments (CLI overrides environment variables)
  for (size_t i = 0; i < args.size(); ++i) {
    const auto &arg = args[i];
    if (arg == "--port" || arg == "-p") {
      if (i + 1 >= args.size()) {
        return Status::Error(StatusCode::InvalidArgument,
                             "Missing value for --port flag");
      }
      const auto &val_str = args[++i];
      uint32_t val = 0;
            auto [ptr, ec] = std::from_chars(val_str.data(), val_str.data() + val_str.size(), valval = 0;
            auto [p, err] = std::from_chars(val_str.data(), val_str.data() + val_str.size(), val);
            if (err != std::errc() || val == 0 || val > 65535) {
        return Status::Error(StatusCode::InvalidArgument,
                             "Invalid port value: " + val_str);
            }
            cfg.server.port = static_cast<uint16_t>(val);
    } else if (arg == "--host") {
      if (i + 1 >= args.size()) {
        return Status::Error(StatusCode::InvalidArgument,
                             "Missing value for --host flag");
      }
      cfg.server.host = args[++i];
    } else if (arg == "--storage-root") {
      if (i + 1 >= args.size()) {
        return Status::Error(StatusCode::InvalidArgument,
                             "Missing value for --storage-root flag");
      }
      cfg.storage.storage_root = args[++i];
    } else if (arg == "--durability") {
      if (i + 1 >= args.size()) {
        return Status::Error(StatusCode::InvalidArgument,
                             "Missing value for --durability flag");
      }
      auto dur_res = parse_durability_profile(args[++i]);
      if (!dur_res.ok())
        return dur_res.status();
      cfg.storage.durability = dur_res.value();
    } else if (arg == "--proxy-port") {
      if (i + 1 >= args.size()) {
        return Status::Error(StatusCode::InvalidArgument,
                             "Missing value for --proxy-port flag");
      }
      const auto &val_str = args[++i];
      uint32_t val = 0;
      auto [p, err] =
          std::from_chars(val_str.data(), val_str.data() + val_str.size(), val);
      if (err != std::errc() || val == 0 || val > 65535) {
        return Status::Error(StatusCode::InvalidArgument,
                             "Invalid proxy-port value: " + val_str);
      }
      cfg.proxy.listen_port = static_cast<uint16_t>(val);
    } else if (arg == "--upstream-host") {
      if (i + 1 >= args.size()) {
        return Status::Error(StatusCode::InvalidArgument,
                             "Missing value for --upstream-host flag");
      }
      cfg.proxy.upstream_host = args[++i];
    } else if (arg == "--upstream-port") {
      if (i + 1 >= args.size()) {
        return Status::Error(StatusCode::InvalidArgument,
                             "Missing value for --upstream-port flag");
      }
      const auto &val_str = args[++i];
      uint32_t val = 0;
      auto [p, err] =
          std::from_chars(val_str.data(), val_str.data() + val_str.size(), val);
      if (err != std::errc() || val == 0 || val > 65535) {
        return Status::Error(StatusCode::InvalidArgument,
                             "Invalid upstream-port value: " + val_str);
      }
      cfg.proxy.upstream_port = static_cast<uint16_t>(val);
    } else {
      return Status::Error(StatusCode::InvalidArgument,
                           "Unknown command line argument: " + arg);
    }
  }

  auto val_status = validate_app_config(cfg);
  if (!val_status.ok()) {
    return val_status;
  }
  return cfg;
}

} // namespace localhive::core::config
