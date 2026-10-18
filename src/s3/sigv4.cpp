#include "localhive/s3/sigv4.hpp"

#include "localhive/core/crypto/crypto.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>

namespace localhive::s3 {
namespace {

std::string trim_and_collapse(std::string_view value) {
    std::string result;
    bool pending_space = false;
    for (const unsigned char character : value) {
        if (std::isspace(character)) { pending_space = !result.empty(); continue; }
        if (pending_space) result.push_back(' ');
        result.push_back(static_cast<char>(character));
        pending_space = false;
    }
    return result;
}

std::string lowercase(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::vector<std::string> split(std::string_view value, char delimiter) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= value.size()) {
        const size_t end = value.find(delimiter, start);
        parts.emplace_back(value.substr(start, end == std::string_view::npos ? end : end - start));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return parts;
}

int hex_value(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

core::Result<std::string> decode_component(std::string_view value) {
    std::string result;
    for (size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '%') { result.push_back(value[index]); continue; }
        if (index + 2 >= value.size()) return core::Status::Error(core::StatusCode::InvalidArgument, "Malformed query percent escape");
        const int high = hex_value(value[index + 1]); const int low = hex_value(value[index + 2]);
        if (high < 0 || low < 0) return core::Status::Error(core::StatusCode::InvalidArgument, "Malformed query percent escape");
        result.push_back(static_cast<char>((high << 4) | low)); index += 2;
    }
    return result;
}

std::string aws_encode(std::string_view value) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    for (const unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') encoded.push_back(static_cast<char>(c));
        else { encoded.push_back('%'); encoded.push_back(hex[c >> 4]); encoded.push_back(hex[c & 15]); }
    }
    return encoded;
}

core::Result<std::string> canonical_query(std::string_view raw_query) {
    std::vector<std::pair<std::string, std::string>> parameters;
    if (raw_query.empty()) return std::string{};
    for (const auto& part : split(raw_query, '&')) {
        const auto equals = part.find('=');
        auto name = decode_component(part.substr(0, equals)); if (!name.ok()) return name.status();
        auto value = decode_component(equals == std::string::npos ? "" : std::string_view(part).substr(equals + 1));
        if (!value.ok()) return value.status();
        parameters.emplace_back(aws_encode(name.value()), aws_encode(value.value()));
    }
    std::sort(parameters.begin(), parameters.end());
    std::ostringstream output;
    for (size_t index = 0; index < parameters.size(); ++index) {
        if (index != 0) output << '&';
        output << parameters[index].first << '=' << parameters[index].second;
    }
    return output.str();
}

core::Result<std::map<std::string, std::string>> canonical_header_map(
    const HttpHeaders& headers, std::string_view signed_headers) {
    std::map<std::string, std::string> normalized;
    for (const auto& header : headers) {
        const auto name = lowercase(header.first);
        const auto value = trim_and_collapse(header.second);
        auto found = normalized.find(name);
        if (found == normalized.end()) normalized.emplace(name, value);
        else found->second += ',' + value;
    }
    for (const auto& name : split(signed_headers, ';')) {
        if (name.empty() || normalized.find(name) == normalized.end()) {
            return core::Status::Error(core::StatusCode::InvalidArgument,
                                       "Signed header is absent from request", name);
        }
    }
    return normalized;
}

core::Result<SigV4Identity> parse_authorization(std::string_view value,
                                                  std::string& signature) {
    constexpr std::string_view algorithm = "AWS4-HMAC-SHA256 ";
    if (value.substr(0, algorithm.size()) != algorithm) {
        return core::Status::Error(core::StatusCode::Unauthenticated, "Unsupported authorization algorithm");
    }
    std::map<std::string, std::string> fields;
    for (const auto& field : split(value.substr(algorithm.size()), ',')) {
        const auto equals = field.find('=');
        if (equals == std::string::npos) return core::Status::Error(core::StatusCode::InvalidArgument, "Malformed authorization field");
        fields.emplace(trim_and_collapse(field.substr(0, equals)), trim_and_collapse(field.substr(equals + 1)));
    }
    const auto credential = fields.find("Credential");
    const auto signed_headers = fields.find("SignedHeaders");
    const auto signature_field = fields.find("Signature");
    if (credential == fields.end() || signed_headers == fields.end() || signature_field == fields.end()) {
        return core::Status::Error(core::StatusCode::InvalidArgument, "Authorization is missing required fields");
    }
    const auto scope = split(credential->second, '/');
    if (scope.size() != 5 || scope[4] != "aws4_request") {
        return core::Status::Error(core::StatusCode::InvalidArgument, "Invalid credential scope");
    }
    signature = signature_field->second;
    return SigV4Identity{scope[0], scope[1], scope[2], scope[3], signed_headers->second};
}

std::string hmac_hex(const std::array<uint8_t, 32>& key, std::string_view data) {
    const auto digest = core::crypto::hmac_sha256(key.data(), key.size(),
                                                   reinterpret_cast<const uint8_t*>(data.data()), data.size());
    return core::crypto::to_hex(digest.data(), digest.size());
}

std::array<uint8_t, 32> hmac_bytes(const uint8_t* key, size_t key_size, std::string_view data) {
    return core::crypto::hmac_sha256(key, key_size,
        reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

} // namespace

core::Result<std::string> canonical_request(const SigV4Request& request,
                                             std::string_view signed_headers) {
    if (request.method.empty() || request.canonical_uri.empty() || request.payload_hash.empty()) {
        return core::Status::Error(core::StatusCode::InvalidArgument, "SigV4 request is missing canonical fields");
    }
    auto headers = canonical_header_map(request.headers, signed_headers); if (!headers.ok()) return headers.status();
    auto query = canonical_query(request.raw_query); if (!query.ok()) return query.status();

    // SigV4 hashes this exact six-line representation. Header names must be
    // sorted and limited to the explicitly declared signed-header list.
    std::ostringstream output;
    output << request.method << '\n' << request.canonical_uri << '\n' << query.value() << '\n';
    for (const auto& name : split(signed_headers, ';')) output << name << ':' << headers.value().at(name) << '\n';
    output << '\n' << signed_headers << '\n' << request.payload_hash;
    return output.str();
}

core::Result<SigV4Identity> verify_sigv4(const SigV4Request& request,
                                          const SigV4Credentials& credentials,
                                          std::string_view authorization_header) {
    std::string supplied_signature;
    auto identity = parse_authorization(authorization_header, supplied_signature);
    if (!identity.ok()) return identity.status();
    if (identity.value().access_key_id != credentials.access_key_id) {
        return core::Status::Error(core::StatusCode::Unauthenticated, "Unknown access key");
    }
    auto canonical = canonical_request(request, identity.value().signed_headers);
    if (!canonical.ok()) return canonical.status();
    const auto timestamp = std::find_if(request.headers.begin(), request.headers.end(),
        [](const auto& header) { return lowercase(header.first) == "x-amz-date"; });
    if (timestamp == request.headers.end() || timestamp->second.size() < 8) {
        return core::Status::Error(core::StatusCode::InvalidArgument, "x-amz-date header is required");
    }
    if (timestamp->second.substr(0, 8) != identity.value().date) {
        return core::Status::Error(core::StatusCode::Unauthenticated, "Credential date does not match x-amz-date");
    }
    const std::string scope = identity.value().date + '/' + identity.value().region + '/' + identity.value().service + "/aws4_request";
    const std::string string_to_sign = "AWS4-HMAC-SHA256\n" + timestamp->second + '\n' + scope + '\n' +
        core::crypto::sha256_hex(canonical.value());

    // Derive the signing key exactly once for this request: date -> region ->
    // service -> terminal aws4_request scope.
    const std::string date_key_secret = "AWS4" + credentials.secret_access_key;
    const auto date_key = hmac_bytes(reinterpret_cast<const uint8_t*>(date_key_secret.data()),
                                     date_key_secret.size(), identity.value().date);
    const auto region_key = hmac_bytes(date_key.data(), date_key.size(), identity.value().region);
    const auto service_key = hmac_bytes(region_key.data(), region_key.size(), identity.value().service);
    const auto signing_key = hmac_bytes(service_key.data(), service_key.size(), "aws4_request");
    const auto expected_signature = hmac_hex(signing_key, string_to_sign);
    if (!core::crypto::constant_time_equals(expected_signature, supplied_signature)) {
        return core::Status::Error(core::StatusCode::Unauthenticated, "SigV4 signature does not match");
    }
    return identity;
}

} // namespace localhive::s3
