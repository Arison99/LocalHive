#include "localhive/core/crypto/crypto.hpp"
#include "localhive/s3/http_handler.hpp"
#include "localhive/s3/service.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace localhive::core;
using namespace localhive::s3;
using namespace localhive::storage;

namespace {

class MemoryMetadataStore final : public IMetadataStore {
public:
    Status initialize() override { return Status::Ok(); }

    Result<BucketRecord> create_bucket(std::string name) override {
        if (buckets_.find(name) != buckets_.end()) {
            return Status::Error(StatusCode::AlreadyExists, "Bucket already exists");
        }
        BucketRecord bucket{next_bucket_id_++, std::move(name)};
        buckets_.emplace(bucket.name, bucket);
        return bucket;
    }

    Result<BucketRecord> find_bucket(std::string_view name) override {
        const auto found = buckets_.find(std::string(name));
        if (found == buckets_.end()) {
            return Status::Error(StatusCode::NotFound, "Bucket does not exist");
        }
        return found->second;
    }

    Result<ObjectRecord> find_or_create_object(BucketId bucket_id,
                                                std::string_view key) override {
        const auto identity = std::make_pair(bucket_id, std::string(key));
        auto found = objects_.find(identity);
        if (found == objects_.end()) {
            ObjectRecord object{next_object_id_++, bucket_id, std::string(key), std::nullopt};
            found = objects_.emplace(identity, std::move(object)).first;
        }
        return found->second;
    }

    Status create_generation(const GenerationRecord& generation) override {
        if (!generations_.emplace(generation.identity.generation_id, generation).second) {
            return Status::Error(StatusCode::AlreadyExists, "Generation already exists");
        }
        return Status::Ok();
    }

    Status set_generation_state(GenerationId generation_id,
                                GenerationState state) override {
        const auto found = generations_.find(generation_id);
        if (found == generations_.end()) return Status::Error(StatusCode::NotFound, "Generation not found");
        found->second.state = state;
        return Status::Ok();
    }

    Status stage_generation(GenerationId generation_id, uint64_t content_length,
                            std::string_view checksum_sha256) override {
        const auto found = generations_.find(generation_id);
        if (found == generations_.end() || found->second.state != GenerationState::Receiving) {
            return Status::Error(StatusCode::FailedPrecondition, "Generation is not receiving");
        }
        found->second.state = GenerationState::Staged;
        found->second.content_length = content_length;
        found->second.checksum_sha256 = std::string(checksum_sha256);
        return Status::Ok();
    }

    Status publish_generation(GenerationId generation_id) override {
        const auto found = generations_.find(generation_id);
        if (found == generations_.end()) return Status::Error(StatusCode::NotFound, "Generation not found");
        auto object = std::find_if(objects_.begin(), objects_.end(), [&](const auto& entry) {
            return entry.second.id == found->second.identity.object_id;
        });
        if (object == objects_.end()) return Status::Error(StatusCode::NotFound, "Object not found");
        if (object->second.active_generation_id) {
            generations_.at(*object->second.active_generation_id).state = GenerationState::Obsolete;
        }
        found->second.state = GenerationState::Published;
        object->second.active_generation_id = generation_id;
        return Status::Ok();
    }

    Result<GenerationRecord> active_generation(BucketId bucket_id,
                                                std::string_view key) override {
        const auto object = objects_.find(std::make_pair(bucket_id, std::string(key)));
        if (object == objects_.end() || !object->second.active_generation_id) {
            return Status::Error(StatusCode::NotFound, "Object has no active generation");
        }
        return generations_.at(*object->second.active_generation_id);
    }

    Status delete_active_object(BucketId bucket_id, std::string_view key) override {
        const auto object = objects_.find(std::make_pair(bucket_id, std::string(key)));
        if (object == objects_.end() || !object->second.active_generation_id) {
            return Status::Error(StatusCode::NotFound, "Object has no active generation");
        }
        generations_.at(*object->second.active_generation_id).state = GenerationState::Tombstone;
        object->second.active_generation_id.reset();
        return Status::Ok();
    }

    Result<std::vector<RecoveryCandidate>> recovery_candidates() override {
        return std::vector<RecoveryCandidate>{};
    }

    Result<std::vector<RecoveryCandidate>> reclaimable_generations() override {
        std::vector<RecoveryCandidate> candidates;
        for (const auto& [id, generation] : generations_) {
            if (generation.state == GenerationState::Obsolete ||
                generation.state == GenerationState::Tombstone) {
                candidates.push_back({generation.identity, generation.state});
            }
        }
        return candidates;
    }

    Status remove_generation(GenerationId generation_id) override {
        if (generations_.erase(generation_id) == 0) {
            return Status::Error(StatusCode::NotFound, "Generation not found");
        }
        return Status::Ok();
    }

private:
    BucketId next_bucket_id_{1};
    ObjectId next_object_id_{1};
    std::map<std::string, BucketRecord> buckets_;
    std::map<std::pair<BucketId, std::string>, ObjectRecord> objects_;
    std::map<GenerationId, GenerationRecord> generations_;
};

Result<DispatchResult> route(const S3Router& router, std::string_view method,
                             std::string_view target_path) {
    auto target = router.parse("localhost:8080", target_path);
    if (!target.ok()) return target.status();
    return dispatch_request(method, std::move(target).value());
}

std::string read_all(ObjectStore::ObjectReadSession& reader) {
    uint8_t buffer[8];
    std::string result;
    while (true) {
        auto count = reader.read(buffer, sizeof(buffer));
        assert(count.ok());
        if (count.value() == 0) break;
        result.append(reinterpret_cast<const char*>(buffer), count.value());
    }
    return result;
}

void test_bucket_create_put_get_and_reader_snapshot() {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() /
                      ("localhive-s3-service-" + std::to_string(nonce));
    auto metadata = std::make_shared<MemoryMetadataStore>();
    S3Service service(root, metadata, config::DurabilityProfile::Development);
    assert(service.initialize().ok());

    const S3Router router;
    auto create = route(router, "PUT", "/demo");
    assert(create.ok());
    assert(service.create_bucket(create.value()).ok());

    {
        auto put = route(router, "PUT", "/demo/docs/item.txt");
        assert(put.ok());
        auto first_writer = service.begin_put(put.value());
        assert(first_writer.ok());
        const std::string first_content = "first generation";
        assert(first_writer.value().write(
            reinterpret_cast<const uint8_t*>(first_content.data()), first_content.size()).ok());
        assert(first_writer.value().finish(crypto::sha256_hex(first_content)).ok());

        auto get = route(router, "GET", "/demo/docs/item.txt");
        assert(get.ok());
        auto old_reader = service.open_get(get.value());
        assert(old_reader.ok());

        auto second_writer = service.begin_put(put.value());
        assert(second_writer.ok());
        const std::string second_content = "second generation";
        assert(second_writer.value().write(
            reinterpret_cast<const uint8_t*>(second_content.data()), second_content.size()).ok());
        assert(second_writer.value().finish(crypto::sha256_hex(second_content)).ok());

        auto current_reader = service.open_get(get.value());
        assert(current_reader.ok());
        assert(read_all(current_reader.value()) == second_content);

        assert(service.delete_object(route(router, "DELETE", "/demo/docs/item.txt").value()).ok());
        assert(read_all(old_reader.value()) == first_content);
        auto deleted_reader = service.open_get(get.value());
        assert(!deleted_reader.ok());
        assert(deleted_reader.status().code() == StatusCode::NotFound);
        assert(service.delete_object(route(router, "DELETE", "/demo/docs/item.txt").value()).ok());

        auto wrong_operation = service.open_get(put.value());
        assert(!wrong_operation.ok());
        assert(wrong_operation.status().code() == StatusCode::FailedPrecondition);

        auto missing = route(router, "GET", "/absent/key");
        assert(missing.ok());
        auto missing_result = service.open_get(missing.value());
        assert(!missing_result.ok());
        assert(missing_result.status().code() == StatusCode::NotFound);
    }

    std::filesystem::remove_all(root);
}

void test_http_handler_streams_put_and_get() {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() /
                      ("localhive-s3-http-" + std::to_string(nonce));
    auto metadata = std::make_shared<MemoryMetadataStore>();
    S3Service service(root, metadata, config::DurabilityProfile::Development);
    assert(service.initialize().ok());
    S3HttpHandler handler(service, [](const HttpRequest&, const S3Target&) {
        return Status::Ok();
    }, S3Router{}, 1024);

    HttpRequest create;
    create.method = "PUT";
    create.host = "localhost:8080";
    create.request_target = "/http-bucket";
    auto created = handler.handle(create);
    assert(created.status_code == 200);

    const std::string payload = "streamed object body";
    size_t offset = 0;
    HttpRequest put;
    put.method = "PUT";
    put.host = "localhost:8080";
    put.request_target = "/http-bucket/file.txt";
    put.content_length = payload.size();
    put.verified_payload_sha256 = crypto::sha256_hex(payload);
    put.read_body = [&payload, &offset](uint8_t* destination, size_t capacity) -> Result<size_t> {
        const size_t count = std::min<size_t>({capacity, 5, payload.size() - offset});
        std::copy_n(reinterpret_cast<const uint8_t*>(payload.data() + offset), count, destination);
        offset += count;
        return count;
    };
    auto uploaded = handler.handle(put);
    assert(uploaded.status_code == 200);

    HttpRequest oversized = put;
    oversized.content_length = 1025;
    auto too_large = handler.handle(oversized);
    assert(too_large.status_code == 413);
    assert(too_large.body.find("<Code>EntityTooLarge</Code>") != std::string::npos);

    HttpRequest truncated = put;
    truncated.request_target = "/http-bucket/truncated.txt";
    truncated.content_length = 4;
    truncated.verified_payload_sha256 = crypto::sha256_hex("");
    truncated.read_body = [](uint8_t*, size_t) -> Result<size_t> { return size_t{0}; };
    auto incomplete = handler.handle(truncated);
    assert(incomplete.status_code == 400);
    HttpRequest absent_object;
    absent_object.method = "GET";
    absent_object.host = "localhost:8080";
    absent_object.request_target = "/http-bucket/truncated.txt";
    auto absent = handler.handle(absent_object);
    assert(absent.status_code == 404);
    assert(absent.body.find("<Code>NoSuchKey</Code>") != std::string::npos);

    HttpRequest get;
    get.method = "GET";
    get.host = "localhost:8080";
    get.request_target = "/http-bucket/file.txt";
    auto downloaded = handler.handle(get);
    assert(downloaded.status_code == 200);
    assert(std::find_if(downloaded.headers.begin(), downloaded.headers.end(), [&payload](const auto& header) {
        return header.first == "Content-Length" && header.second == std::to_string(payload.size());
    }) != downloaded.headers.end());
    std::string received;
    const auto streamed = downloaded.stream_body([&received](const uint8_t* data, size_t size) {
        received.append(reinterpret_cast<const char*>(data), size);
        return Status::Ok();
    });
    assert(streamed.ok());
    assert(received == payload);
    downloaded.stream_body = {};

    HttpRequest missing_bucket;
    missing_bucket.method = "GET";
    missing_bucket.host = "localhost:8080";
    missing_bucket.request_target = "/absent/file.txt";
    auto missing = handler.handle(missing_bucket);
    assert(missing.status_code == 404);
    assert(missing.body.find("<Code>NoSuchBucket</Code>") != std::string::npos);

    HttpRequest unsupported_list;
    unsupported_list.method = "GET";
    unsupported_list.host = "localhost:8080";
    unsupported_list.request_target = "/http-bucket?list-type=2";
    auto unsupported = handler.handle(unsupported_list);
    assert(unsupported.status_code == 501);

    HttpRequest header_injection;
    header_injection.method = "PUT";
    header_injection.host = "localhost:8080";
    header_injection.request_target = "/bad%0D%0AX%3Avalue";
    auto encoded_location = handler.handle(header_injection);
    assert(encoded_location.status_code == 200);
    assert(std::find_if(encoded_location.headers.begin(), encoded_location.headers.end(),
        [](const auto& header) {
            return header.first == "Location" &&
                   header.second.find_first_of("\r\n") == std::string::npos;
        }) != encoded_location.headers.end());

    S3HttpHandler denied_handler(service, [](const HttpRequest&, const S3Target&) {
        return Status::Error(StatusCode::Unauthenticated, "Invalid credentials");
    });
    auto denied = denied_handler.handle(get);
    assert(denied.status_code == 403);
    assert(denied.body.find("<Code>AccessDenied</Code>") != std::string::npos);

    HttpRequest delete_request;
    delete_request.method = "DELETE";
    delete_request.host = "localhost:8080";
    delete_request.request_target = "/http-bucket/file.txt";
    auto deleted = handler.handle(delete_request);
    assert(deleted.status_code == 204);
    auto after_delete = handler.handle(get);
    assert(after_delete.status_code == 404);
    assert(after_delete.body.find("<Code>NoSuchKey</Code>") != std::string::npos);
    assert(handler.handle(delete_request).status_code == 204);

    std::filesystem::remove_all(root);
}

} // namespace

int main() {
    test_bucket_create_put_get_and_reader_snapshot();
    test_http_handler_streams_put_and_get();
}