#include "localhive/s3/xml.hpp"

#include <sstream>

namespace localhive::s3::xml {
namespace {

constexpr const char* kXmlDeclaration = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
constexpr const char* kS3Namespace = "http://s3.amazonaws.com/doc/2006-03-01/";

void element(std::ostringstream& output, std::string_view name, std::string_view value) {
    output << '<' << name << '>' << escape(value) << "</" << name << ">\n";
}

} // namespace

std::string escape(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '\"': escaped += "&quot;"; break;
        case '\'': escaped += "&apos;"; break;
        default: escaped.push_back(character); break;
        }
    }
    return escaped;
}

std::string error_response(const Error& error) {
    std::ostringstream output;
    output << kXmlDeclaration << "<Error>\n";
    element(output, "Code", error.code);
    element(output, "Message", error.message);
    if (error.bucket_name) element(output, "BucketName", *error.bucket_name);
    if (error.key) element(output, "Key", *error.key);
    if (!error.resource.empty()) element(output, "Resource", error.resource);
    if (!error.request_id.empty()) element(output, "RequestId", error.request_id);
    if (!error.host_id.empty()) element(output, "HostId", error.host_id);
    output << "</Error>\n";
    return output.str();
}

std::string list_buckets_response(const std::vector<Bucket>& buckets,
                                  std::string_view owner_id,
                                  std::string_view owner_name) {
    std::ostringstream output;
    output << kXmlDeclaration << "<ListAllMyBucketsResult xmlns=\"" << kS3Namespace << "\">\n";
    output << "<Owner>\n";
    element(output, "ID", owner_id);
    element(output, "DisplayName", owner_name);
    output << "</Owner>\n<Buckets>\n";
    for (const auto& bucket : buckets) {
        output << "<Bucket>\n";
        element(output, "Name", bucket.name);
        element(output, "CreationDate", bucket.creation_date);
        output << "</Bucket>\n";
    }
    output << "</Buckets>\n</ListAllMyBucketsResult>\n";
    return output.str();
}

std::string list_objects_v2_response(const ListObjectsV2& page) {
    std::ostringstream output;
    output << kXmlDeclaration << "<ListBucketResult xmlns=\"" << kS3Namespace << "\">\n";
    element(output, "Name", page.bucket);
    element(output, "Prefix", page.prefix);
    if (page.continuation_token) element(output, "ContinuationToken", *page.continuation_token);
    if (!page.delimiter.empty()) element(output, "Delimiter", page.delimiter);
    element(output, "KeyCount", std::to_string(page.objects.size() + page.common_prefixes.size()));
    element(output, "MaxKeys", std::to_string(page.max_keys));
    element(output, "IsTruncated", page.is_truncated ? "true" : "false");
    if (page.next_continuation_token) element(output, "NextContinuationToken", *page.next_continuation_token);

    // Contents and common prefixes are separate result entries; both count
    // toward KeyCount, which is why it is calculated above rather than from
    // the object vector alone.
    for (const auto& object : page.objects) {
        output << "<Contents>\n";
        element(output, "Key", object.key);
        element(output, "LastModified", object.last_modified);
        element(output, "ETag", object.etag);
        element(output, "Size", std::to_string(object.size));
        element(output, "StorageClass", "STANDARD");
        output << "</Contents>\n";
    }
    for (const auto& prefix : page.common_prefixes) {
        output << "<CommonPrefixes>\n";
        element(output, "Prefix", prefix);
        output << "</CommonPrefixes>\n";
    }
    output << "</ListBucketResult>\n";
    return output.str();
}

} // namespace localhive::s3::xml
