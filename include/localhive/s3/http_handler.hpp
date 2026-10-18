#ifndef LOCALHIVE_S3_HTTP_HANDLER_HPP
#define LOCALHIVE_S3_HTTP_HANDLER_HPP

#include "localhive/core/types.hpp"
#include "localhive/s3/request_dispatch.hpp"
#include "localhive/s3/service.hpp"
#include "localhive/s3/sigv4.hpp"
#include <functional>
#include <string>

namespace localhive::s3 {

using HttpBodyReader = std::function<core::Result<size_t>(uint8_t*, size_t)>;
using HttpBodyWriter = std::function<core::Status(const uint8_t*, size_t)>;

struct HttpRequest {
    std::string method;
    std::string host;
    std::string request_target;
    HttpHeaders headers;
    uint64_t content_length{0};
    HttpBodyReader read_body;
    // The transport/authentication layer must verify this digest while reading
    // the request body before passing it to the storage service.
    std::string verified_payload_sha256;
};

struct HttpResponse {
    unsigned status_code{500};
    HttpHeaders headers;
    std::string body;
    std::function<core::Status(const HttpBodyWriter&)> stream_body;
};

class S3HttpHandler {
public:
    using Authorizer = std::function<core::Status(const HttpRequest&, const S3Target&)>;

    S3HttpHandler(S3Service& service, Authorizer authorize,
                  S3Router router = S3Router{},
                  uint64_t max_put_body_bytes = 5ULL * 1024 * 1024 * 1024);

    [[nodiscard]] HttpResponse handle(const HttpRequest& request);

private:
    S3Service& service_;
    Authorizer authorize_;
    S3Router router_;
    uint64_t max_put_body_bytes_;
};

} // namespace localhive::s3

#endif // LOCALHIVE_S3_HTTP_HANDLER_HPP