#include "localhive/s3/http_handler.hpp"

#include "localhive/s3/xml.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <memory>
#include <utility>

namespace localhive::s3 {
namespace {

HttpResponse error_response(unsigned status, std::string code, std::string message,
                            std::string resource = {}) {
    xml::Error error;
    error.code = std::move(code);
    error.message = std::move(message);
    error.resource = std::move(resource);
    return {status, {{"Content-Type", "application/xml"}}, xml::error_response(error), {}};
}

HttpResponse status_error(const core::Status& status, std::string resource = {}) {
    switch (status.code()) {
    case core::StatusCode::InvalidArgument:
        return error_response(400, "InvalidArgument", status.message(), std::move(resource));
    case core::StatusCode::NotFound:
        return error_response(404, status.detail() == "bucket" ? "NoSuchBucket" : "NoSuchKey",
                              status.message(), std::move(resource));
    case core::StatusCode::AlreadyExists:
        return error_response(409, "BucketAlreadyExists", status.message(), std::move(resource));
    case core::StatusCode::Unauthenticated:
    case core::StatusCode::PermissionDenied:
        return error_response(403, "AccessDenied", status.message(), std::move(resource));
    case core::StatusCode::ResourceExhausted:
        return error_response(503, "SlowDown", status.message(), std::move(resource));
    case core::StatusCode::Unimplemented:
        return error_response(501, "NotImplemented", status.message(), std::move(resource));
    case core::StatusCode::FailedPrecondition:
        return error_response(400, "InvalidRequest", status.message(), std::move(resource));
    case core::StatusCode::DataLoss:
        return error_response(400, "InvalidRequest", status.message(), std::move(resource));
    default:
        return error_response(500, "InternalError", status.message(), std::move(resource));
    }
}

HttpResponse not_implemented(std::string resource) {
    return error_response(501, "NotImplemented",
                          "The operation is not implemented by this service",
                          std::move(resource));
}

std::string encode_path_component(std::string_view value) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    for (const unsigned char character : value) {
        if (std::isalnum(character) || character == '-' || character == '_' ||
            character == '.' || character == '~') {
            encoded.push_back(static_cast<char>(character));
        } else {
            encoded.push_back('%');
            encoded.push_back(hex[character >> 4]);
            encoded.push_back(hex[character & 0x0f]);
        }
    }
    return encoded;
}

} // namespace

S3HttpHandler::S3HttpHandler(S3Service& service, Authorizer authorize,
                             S3Router router, uint64_t max_put_body_bytes)
    : service_(service), authorize_(std::move(authorize)), router_(std::move(router)),
      max_put_body_bytes_(max_put_body_bytes) {}

HttpResponse S3HttpHandler::handle(const HttpRequest& request) {
    auto target = router_.parse(request.host, request.request_target);
    if (!target.ok()) return status_error(target.status(), request.request_target);

    if (!authorize_) {
        return status_error(core::Status::Error(core::StatusCode::Unauthenticated,
                                                "No S3 authorizer is configured"),
                            request.request_target);
    }
    const auto authorization = authorize_(request, target.value());
    if (!authorization.ok()) return status_error(authorization, request.request_target);

    auto dispatched = dispatch_request(request.method, std::move(target).value());
    if (!dispatched.ok()) return status_error(dispatched.status(), request.request_target);
    const auto& operation = dispatched.value();

    if (operation.operation == Operation::CreateBucket) {
        auto bucket = service_.create_bucket(operation);
        if (!bucket.ok()) return status_error(bucket.status(), request.request_target);
        return {200, {{"Location", "/" + encode_path_component(bucket.value().name)},
                  {"Content-Length", "0"}}, "", {}};
    }

    if (operation.operation == Operation::PutObject) {
        if (request.content_length > max_put_body_bytes_) {
            return error_response(413, "EntityTooLarge",
                                  "Object exceeds configured maximum size",
                                  request.request_target);
        }
        if (!request.read_body || request.verified_payload_sha256.empty()) {
            return status_error(core::Status::Error(core::StatusCode::InvalidArgument,
                                                   "PUT requires a body reader and verified SHA-256 digest"),
                                request.request_target);
        }
        auto writer = service_.begin_put(operation);
        if (!writer.ok()) return status_error(writer.status(), request.request_target);

        std::array<uint8_t, 64 * 1024> buffer{};
        uint64_t remaining = request.content_length;
        while (remaining != 0) {
            const auto capacity = static_cast<size_t>(std::min<uint64_t>(remaining, buffer.size()));
            auto count = request.read_body(buffer.data(), capacity);
            if (!count.ok()) return status_error(count.status(), request.request_target);
            if (count.value() == 0 || count.value() > capacity) {
                return status_error(core::Status::Error(core::StatusCode::DataLoss,
                                                       "PUT body length does not match Content-Length"),
                                    request.request_target);
            }
            auto status = writer.value().write(buffer.data(), count.value());
            if (!status.ok()) return status_error(status, request.request_target);
            remaining -= count.value();
        }

        auto published = writer.value().finish(request.verified_payload_sha256);
        if (!published.ok()) return status_error(published.status(), request.request_target);
        return {200, {{"Content-Length", "0"}}, "", {}};
    }

    if (operation.operation == Operation::GetObject) {
        auto reader = service_.open_get(operation);
        if (!reader.ok()) return status_error(reader.status(), request.request_target);
        auto session = std::make_shared<storage::ObjectStore::ObjectReadSession>(
            std::move(reader).value());
        HttpResponse response{200,
            {{"Content-Type", "application/octet-stream"},
             {"Content-Length", std::to_string(session->content_length())}},
            "", {}};
        response.stream_body = [session](const HttpBodyWriter& write) {
            if (!write) return core::Status::Error(core::StatusCode::InvalidArgument,
                                                   "Response body writer cannot be empty");
            std::array<uint8_t, 64 * 1024> buffer{};
            while (true) {
                auto count = session->read(buffer.data(), buffer.size());
                if (!count.ok()) return count.status();
                if (count.value() == 0) return core::Status::Ok();
                auto status = write(buffer.data(), count.value());
                if (!status.ok()) return status;
            }
        };
        return response;
    }

    if (operation.operation == Operation::DeleteObject) {
        auto status = service_.delete_object(operation);
        if (!status.ok()) return status_error(status, request.request_target);
        return {204, {{"Content-Length", "0"}}, "", {}};
    }

    return not_implemented(request.request_target);
}

} // namespace localhive::s3