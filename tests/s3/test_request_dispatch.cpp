#include "localhive/s3/request_dispatch.hpp"
#include <cassert>
#include <string_view>
#include <utility>

using namespace localhive::core;
using namespace localhive::s3;

namespace {

Result<DispatchResult> dispatch_path(const S3Router& router,
                                    std::string_view method,
                                    std::string_view request_target) {
    auto target = router.parse("localhost:8080", request_target);
    if (!target.ok()) return target.status();
    return dispatch_request(method, std::move(target).value());
}

void test_operation_selection() {
    const S3Router router;

    assert(dispatch_path(router, "GET", "/").value().operation == Operation::ListBuckets);
    assert(dispatch_path(router, "PUT", "/bucket").value().operation == Operation::CreateBucket);
    assert(dispatch_path(router, "DELETE", "/bucket").value().operation == Operation::DeleteBucket);
    assert(dispatch_path(router, "HEAD", "/bucket").value().operation == Operation::HeadBucket);
    assert(dispatch_path(router, "PUT", "/bucket/key").value().operation == Operation::PutObject);
    assert(dispatch_path(router, "GET", "/bucket/key").value().operation == Operation::GetObject);
    assert(dispatch_path(router, "HEAD", "/bucket/key").value().operation == Operation::HeadObject);
    assert(dispatch_path(router, "DELETE", "/bucket/key").value().operation == Operation::DeleteObject);
    assert(dispatch_path(router, "GET", "/bucket?list-type=2").value().operation == Operation::ListObjectsV2);
    assert(dispatch_path(router, "POST", "/bucket?delete").value().operation == Operation::DeleteObjects);
}

void test_object_route_precedes_bucket_selector() {
    const S3Router router;
    auto result = dispatch_path(router, "GET", "/bucket/key?list-type=2");

    assert(result.ok());
    assert(result.value().operation == Operation::GetObject);
}

void test_query_parameters_are_decoded() {
    const S3Router router;
    auto result = dispatch_path(router, "GET", "/bucket?list-type=2&prefix=a%2Bb");

    assert(result.ok());
    assert(result.value().operation == Operation::ListObjectsV2);
    assert(result.value().query_parameters.at("prefix") == "a+b");
}

void test_duplicate_decoded_query_parameter_is_rejected() {
    const S3Router router;
    auto result = dispatch_path(router, "GET", "/bucket?prefix=a&%70refix=b");

    assert(!result.ok());
    assert(result.status().code() == StatusCode::InvalidArgument);
}

void test_malformed_query_escape_is_rejected() {
    const S3Router router;
    auto result = dispatch_path(router, "GET", "/bucket?prefix=%Q0");

    assert(!result.ok());
    assert(result.status().code() == StatusCode::InvalidArgument);
}

void test_unsupported_operation_is_rejected() {
    const S3Router router;
    auto result = dispatch_path(router, "GET", "/bucket?prefix=docs/");

    assert(!result.ok());
    assert(result.status().code() == StatusCode::Unimplemented);
}

void test_method_token_is_case_sensitive() {
    const S3Router router;
    auto result = dispatch_path(router, "get", "/");

    assert(!result.ok());
    assert(result.status().code() == StatusCode::Unimplemented);
}

} // namespace

int main() {
    test_operation_selection();
    test_object_route_precedes_bucket_selector();
    test_query_parameters_are_decoded();
    test_duplicate_decoded_query_parameter_is_rejected();
    test_malformed_query_escape_is_rejected();
    test_unsupported_operation_is_rejected();
    test_method_token_is_case_sensitive();
}