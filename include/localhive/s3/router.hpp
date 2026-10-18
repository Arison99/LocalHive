#ifndef LOCALHIVE_S3_ROUTER_HPP
#define LOCALHIVE_S3_ROUTER_HPP

#include "localhive/core/types.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace localhive::s3 {

enum class AddressingStyle { Path, VirtualHosted };

struct S3Target {
    AddressingStyle style{AddressingStyle::Path};
    std::optional<std::string> bucket;
    std::optional<std::string> key;
    std::string query;
    std::string canonical_path;
};

struct RouterConfig {
    // Virtual-hosted requests are recognized as <bucket>.<endpoint_host>.
    std::string endpoint_host{"localhost"};
};

class S3Router {
public:
    explicit S3Router(RouterConfig config = {});
    [[nodiscard]] core::Result<S3Target> parse(std::string_view host,
                                                std::string_view request_target) const;

private:
    RouterConfig config_;
};

// Strict URL decoding used for S3 resource components. '+' remains a literal
// plus because S3 paths are not form-encoded.
[[nodiscard]] core::Result<std::string> percent_decode(std::string_view value);

} // namespace localhive::s3

#endif // LOCALHIVE_S3_ROUTER_HPP
