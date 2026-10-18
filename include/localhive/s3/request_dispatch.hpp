#ifndef LOCALHIVE_S3_REQUEST_DISPATCH_HPP
#define LOCALHIVE_S3_REQUEST_DISPATCH_HPP

#include "localhive/core/types.hpp"
#include "localhive/s3/router.hpp"
#include <map>
#include <string>

namespace localhive::s3 {

enum class Operation {
    ListBuckets,
    CreateBucket,
    DeleteBucket,
    HeadBucket,
    PutObject,
    GetObject,
    HeadObject,
    DeleteObject,
    DeleteObjects,
    ListObjectsV2
};

struct DispatchResult {
    Operation operation{Operation::ListBuckets};
    S3Target target;
    // Decoded query parameters. Repeated parameters are rejected because the
    // initial S3 operation surface has no duplicate-parameter semantics.
    std::map<std::string, std::string> query_parameters;
};

[[nodiscard]] core::Result<DispatchResult> dispatch_request(
    std::string_view method, S3Target target);

} // namespace localhive::s3

#endif // LOCALHIVE_S3_REQUEST_DISPATCH_HPP
