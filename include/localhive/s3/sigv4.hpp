#ifndef LOCALHIVE_S3_SIGV4_HPP
#define LOCALHIVE_S3_SIGV4_HPP

#include "localhive/core/types.hpp"
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace localhive::s3 {

using HttpHeaders = std::vector<std::pair<std::string, std::string>>;

struct SigV4Credentials {
    std::string access_key_id;
    std::string secret_access_key;
};

struct SigV4Request {
    std::string method;
    std::string canonical_uri;
    std::string raw_query;
    HttpHeaders headers;
    // Use the x-amz-content-sha256 value, or UNSIGNED-PAYLOAD for supported
    // streaming clients. The verifier does not read the request body itself.
    std::string payload_hash;
};

struct SigV4Identity {
    std::string access_key_id;
    std::string date;
    std::string region;
    std::string service;
    std::string signed_headers;
};

// Returns the canonical request text that is hashed as the first SigV4 step.
[[nodiscard]] core::Result<std::string> canonical_request(
    const SigV4Request& request, std::string_view signed_headers);

// Verifies header-based AWS4-HMAC-SHA256 authorization. Credentials must be
// selected by the caller after parsing the access key from the authorization header.
[[nodiscard]] core::Result<SigV4Identity> verify_sigv4(
    const SigV4Request& request, const SigV4Credentials& credentials,
    std::string_view authorization_header);

} // namespace localhive::s3

#endif // LOCALHIVE_S3_SIGV4_HPP
