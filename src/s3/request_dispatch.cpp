#include "localhive/s3/request_dispatch.hpp"

#include <utility>

namespace localhive::s3 {
namespace {

core::Result<std::map<std::string, std::string>> parse_query(std::string_view query) {
    std::map<std::string, std::string> parameters;
    size_t start = 0;
    while (start < query.size()) {
        const auto end = query.find('&', start);
        const auto part = query.substr(start, end == std::string_view::npos ? end : end - start);
        const auto equals = part.find('=');
        auto key = percent_decode(part.substr(0, equals)); if (!key.ok()) return key.status();
        auto value = percent_decode(equals == std::string_view::npos ? "" : part.substr(equals + 1));
        if (!value.ok()) return value.status();
        if (!parameters.emplace(std::move(key).value(), std::move(value).value()).second) {
            return core::Status::Error(core::StatusCode::InvalidArgument,
                "Duplicate S3 query parameter");
        }
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return parameters;
}

} // namespace

core::Result<DispatchResult> dispatch_request(std::string_view method, S3Target target) {
    auto parameters = parse_query(target.query); if (!parameters.ok()) return parameters.status();
    const bool has_bucket = target.bucket.has_value();
    const bool has_key = target.key.has_value();
    const auto list_type = parameters.value().find("list-type");
    const bool list_v2 = list_type != parameters.value().end() && list_type->second == "2";
    const bool delete_many = parameters.value().find("delete") != parameters.value().end();

    // Object routes have precedence whenever a key is present. This keeps
    // slash-containing object names opaque to bucket-level dispatch rules.
    if (method == "PUT" && has_bucket && has_key) return DispatchResult{Operation::PutObject, std::move(target), std::move(parameters).value()};
    if (method == "GET" && has_bucket && has_key) return DispatchResult{Operation::GetObject, std::move(target), std::move(parameters).value()};
    if (method == "HEAD" && has_bucket && has_key) return DispatchResult{Operation::HeadObject, std::move(target), std::move(parameters).value()};
    if (method == "DELETE" && has_bucket && has_key) return DispatchResult{Operation::DeleteObject, std::move(target), std::move(parameters).value()};

    if (method == "GET" && !has_bucket) return DispatchResult{Operation::ListBuckets, std::move(target), std::move(parameters).value()};
    if (method == "PUT" && has_bucket) return DispatchResult{Operation::CreateBucket, std::move(target), std::move(parameters).value()};
    if (method == "DELETE" && has_bucket) return DispatchResult{Operation::DeleteBucket, std::move(target), std::move(parameters).value()};
    if (method == "HEAD" && has_bucket) return DispatchResult{Operation::HeadBucket, std::move(target), std::move(parameters).value()};
    if (method == "GET" && has_bucket && list_v2) return DispatchResult{Operation::ListObjectsV2, std::move(target), std::move(parameters).value()};
    if (method == "POST" && has_bucket && delete_many) return DispatchResult{Operation::DeleteObjects, std::move(target), std::move(parameters).value()};

    return core::Status::Error(core::StatusCode::Unimplemented,
        "S3 operation is not supported by the current compatibility tier");
}

} // namespace localhive::s3
