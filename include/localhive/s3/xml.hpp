#ifndef LOCALHIVE_S3_XML_HPP
#define LOCALHIVE_S3_XML_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace localhive::s3::xml {

struct Error {
    std::string code;
    std::string message;
    std::string resource;
    std::string request_id;
    std::string host_id;
    std::optional<std::string> bucket_name;
    std::optional<std::string> key;
};

struct Bucket {
    std::string name;
    // ISO 8601 UTC, for example: 2026-10-18T12:34:56.000Z.
    std::string creation_date;
};

struct Object {
    std::string key;
    std::string last_modified;
    std::string etag;
    uint64_t size{};
};

struct ListObjectsV2 {
    std::string bucket;
    std::string prefix;
    std::string delimiter;
    uint32_t max_keys{1000};
    bool is_truncated{false};
    std::optional<std::string> continuation_token;
    std::optional<std::string> next_continuation_token;
    std::vector<Object> objects;
    std::vector<std::string> common_prefixes;
};

// Escapes text before inclusion in an XML element. It deliberately escapes
// both quote variants so callers can safely reuse it in attribute contexts.
[[nodiscard]] std::string escape(std::string_view value);
[[nodiscard]] std::string error_response(const Error& error);
[[nodiscard]] std::string list_buckets_response(const std::vector<Bucket>& buckets,
                                                 std::string_view owner_id = "localhive",
                                                 std::string_view owner_name = "localhive");
[[nodiscard]] std::string list_objects_v2_response(const ListObjectsV2& page);

} // namespace localhive::s3::xml

#endif // LOCALHIVE_S3_XML_HPP
