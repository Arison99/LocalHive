#include "localhive/s3/router.hpp"

#include <algorithm>
#include <cctype>

namespace localhive::s3 {
namespace {

int hex_value(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

std::string lowercase(std::string_view input) {
    std::string output(input);
    std::transform(output.begin(), output.end(), output.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return output;
}

std::string without_port(std::string_view host) {
    if (!host.empty() && host.front() == '[') return std::string(host);
    const auto colon = host.rfind(':');
    return colon == std::string_view::npos ? std::string(host) : std::string(host.substr(0, colon));
}

core::Status validate_bucket(std::string_view bucket) {
    if (bucket.empty() || bucket.size() > 63) {
        return core::Status::Error(core::StatusCode::InvalidArgument, "Invalid S3 bucket name");
    }
    return core::Status::Ok();
}

} // namespace

S3Router::S3Router(RouterConfig config) : config_(std::move(config)) {}

core::Result<std::string> percent_decode(std::string_view value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '%') { decoded.push_back(value[index]); continue; }
        if (index + 2 >= value.size()) {
            return core::Status::Error(core::StatusCode::InvalidArgument, "Malformed URL percent escape");
        }
        const int high = hex_value(value[index + 1]);
        const int low = hex_value(value[index + 2]);
        if (high < 0 || low < 0) {
            return core::Status::Error(core::StatusCode::InvalidArgument, "Malformed URL percent escape");
        }
        const char decoded_byte = static_cast<char>((high << 4) | low);
        if (decoded_byte == '\0') {
            return core::Status::Error(core::StatusCode::InvalidArgument, "URL cannot contain a NUL byte");
        }
        decoded.push_back(decoded_byte);
        index += 2;
    }
    return decoded;
}

core::Result<S3Target> S3Router::parse(std::string_view host,
                                       std::string_view request_target) const {
    if (request_target.empty() || request_target.front() != '/') {
        return core::Status::Error(core::StatusCode::InvalidArgument,
                                   "S3 request target must begin with '/'");
    }
    const auto query_start = request_target.find('?');
    const auto raw_path = request_target.substr(0, query_start);
    const auto query = query_start == std::string_view::npos
        ? std::string{} : std::string(request_target.substr(query_start + 1));
    // Addressing style is selected from the host before parsing the path:
    // bucket.localhost/key is virtual-hosted; localhost/bucket/key is path-style.
    const auto normalized_host = lowercase(without_port(host));
    const auto endpoint = lowercase(config_.endpoint_host);
    const std::string virtual_suffix = "." + endpoint;
    const bool virtual_hosted = normalized_host.size() > virtual_suffix.size() &&
        normalized_host.compare(normalized_host.size() - virtual_suffix.size(),
                                virtual_suffix.size(), virtual_suffix) == 0;

    S3Target result;
    result.query = query;
    result.canonical_path = std::string(raw_path);
    if (virtual_hosted) {
        auto bucket = percent_decode(normalized_host.substr(0, normalized_host.size() - virtual_suffix.size()));
        if (!bucket.ok()) return bucket.status();
        const auto valid = validate_bucket(bucket.value()); if (!valid.ok()) return valid;
        result.style = AddressingStyle::VirtualHosted;
        result.bucket = std::move(bucket).value();
        if (raw_path.size() > 1) {
            auto key = percent_decode(raw_path.substr(1)); if (!key.ok()) return key.status();
            result.key = std::move(key).value();
        }
        return result;
    }

    // The root path represents ListBuckets in path-style addressing.
    if (raw_path == "/") return result;
    const auto bucket_end = raw_path.find('/', 1);
    const auto raw_bucket = raw_path.substr(1, bucket_end == std::string_view::npos
                                                   ? std::string_view::npos : bucket_end - 1);
    auto bucket = percent_decode(raw_bucket); if (!bucket.ok()) return bucket.status();
    const auto valid = validate_bucket(bucket.value()); if (!valid.ok()) return valid;
    result.bucket = std::move(bucket).value();
    if (bucket_end != std::string_view::npos && bucket_end + 1 < raw_path.size()) {
        auto key = percent_decode(raw_path.substr(bucket_end + 1)); if (!key.ok()) return key.status();
        result.key = std::move(key).value();
    }
    return result;
}

} // namespace localhive::s3
