#include "localhive/storage/metadata_store.hpp"

#if defined(LOCALHIVE_HAS_SQLITE3)
#include <sqlite3.h>
#endif

namespace localhive::storage {
namespace {

const char* state_name(GenerationState state) {
    switch (state) {
    case GenerationState::Receiving: return "receiving";
    case GenerationState::Staged: return "staged";
    case GenerationState::Prepared: return "prepared";
    case GenerationState::Published: return "published";
    case GenerationState::Committed: return "committed";
    case GenerationState::Obsolete: return "obsolete";
    case GenerationState::Tombstone: return "tombstone";
    case GenerationState::Aborted: return "aborted";
    }
    return "aborted";
}

core::Result<GenerationState> parse_state(const char* state) {
    if (state == nullptr) return core::Status::Error(core::StatusCode::DataLoss, "Generation state is null");
    const std::string value(state);
    if (value == "receiving") return GenerationState::Receiving;
    if (value == "staged") return GenerationState::Staged;
    if (value == "prepared") return GenerationState::Prepared;
    if (value == "published") return GenerationState::Published;
    if (value == "committed") return GenerationState::Committed;
    if (value == "obsolete") return GenerationState::Obsolete;
    if (value == "tombstone") return GenerationState::Tombstone;
    if (value == "aborted") return GenerationState::Aborted;
    return core::Status::Error(core::StatusCode::DataLoss, "Unknown generation state", value);
}

#if defined(LOCALHIVE_HAS_SQLITE3)
core::Status sqlite_status(sqlite3* db, int result, const char* action) {
    if (result == SQLITE_OK || result == SQLITE_DONE || result == SQLITE_ROW) {
        return core::Status::Ok();
    }
    return core::Status::Error(core::StatusCode::Internal, action,
                               db == nullptr ? "SQLite unavailable" : sqlite3_errmsg(db));
}

core::Status execute(sqlite3* db, const char* sql) {
    char* message = nullptr;
    const int result = sqlite3_exec(db, sql, nullptr, nullptr, &message);
    std::string detail = message == nullptr ? "" : message;
    sqlite3_free(message);
    if (result != SQLITE_OK) {
        return core::Status::Error(core::StatusCode::Internal, "SQLite statement failed", detail);
    }
    return core::Status::Ok();
}

class Statement {
public:
    Statement(sqlite3* db, const char* sql) : db_(db) { result_ = sqlite3_prepare_v2(db, sql, -1, &statement_, nullptr); }
    ~Statement() { sqlite3_finalize(statement_); }
    [[nodiscard]] int result() const noexcept { return result_; }
    [[nodiscard]] sqlite3_stmt* get() const noexcept { return statement_; }
private:
    sqlite3* db_;
    sqlite3_stmt* statement_{nullptr};
    int result_{SQLITE_ERROR};
};
#endif

} // namespace

SqliteMetadataStore::SqliteMetadataStore(std::filesystem::path database_path,
                                         std::string synchronous_mode)
    : database_path_(std::move(database_path)), synchronous_mode_(std::move(synchronous_mode)) {}

SqliteMetadataStore::~SqliteMetadataStore() {
#if defined(LOCALHIVE_HAS_SQLITE3)
    std::lock_guard<std::mutex> lock(mutex_);
    if (database_ != nullptr) sqlite3_close_v2(static_cast<sqlite3*>(database_));
#endif
}

core::Status SqliteMetadataStore::ensure_open_locked() {
#if !defined(LOCALHIVE_HAS_SQLITE3)
    return core::Status::Error(core::StatusCode::Unimplemented,
                               "LocalHive was built without SQLite support");
#else
    if (database_ != nullptr) return core::Status::Ok();
    sqlite3* db = nullptr;
    const int result = sqlite3_open_v2(database_path_.string().c_str(), &db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (result != SQLITE_OK) {
        const auto status = sqlite_status(db, result, "Could not open metadata database");
        sqlite3_close_v2(db);
        return status;
    }
    database_ = db;
    sqlite3_busy_timeout(db, 5000);
    return core::Status::Ok();
#endif
}

core::Status SqliteMetadataStore::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto status = ensure_open_locked();
    if (!status.ok()) return status;
#if defined(LOCALHIVE_HAS_SQLITE3)
    auto* db = static_cast<sqlite3*>(database_);
    status = execute(db, "PRAGMA journal_mode=WAL;");
    if (!status.ok()) return status;
    if (synchronous_mode_ != "FULL" && synchronous_mode_ != "NORMAL" && synchronous_mode_ != "OFF") {
        return core::Status::Error(core::StatusCode::InvalidArgument, "Invalid SQLite synchronous mode");
    }
    status = execute(db, ("PRAGMA synchronous=" + synchronous_mode_ + ";").c_str());
    if (!status.ok()) return status;
    return execute(db,
        "PRAGMA foreign_keys=ON;"
        "CREATE TABLE IF NOT EXISTS buckets (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE);"
        "CREATE TABLE IF NOT EXISTS objects (id INTEGER PRIMARY KEY, bucket_id INTEGER NOT NULL, key TEXT NOT NULL, active_gen_id INTEGER, UNIQUE(bucket_id,key), FOREIGN KEY(bucket_id) REFERENCES buckets(id));"
        "CREATE TABLE IF NOT EXISTS generations (gen_id INTEGER PRIMARY KEY, object_id INTEGER NOT NULL, state TEXT NOT NULL, content_length INTEGER NOT NULL, checksum_sha256 TEXT NOT NULL, FOREIGN KEY(object_id) REFERENCES objects(id));");
#else
    return status;
#endif
}

core::Result<BucketRecord> SqliteMetadataStore::create_bucket(std::string name) {
    if (name.empty()) return core::Status::Error(core::StatusCode::InvalidArgument, "Bucket name cannot be empty");
    std::lock_guard<std::mutex> lock(mutex_);
    auto status = ensure_open_locked(); if (!status.ok()) return status;
#if defined(LOCALHIVE_HAS_SQLITE3)
    auto* db = static_cast<sqlite3*>(database_);
    Statement statement(db, "INSERT INTO buckets(name) VALUES(?1);");
    if (statement.result() != SQLITE_OK) return sqlite_status(db, statement.result(), "Could not prepare bucket insert");
    sqlite3_bind_text(statement.get(), 1, name.c_str(), -1, SQLITE_TRANSIENT);
    const int result = sqlite3_step(statement.get());
    if (result == SQLITE_CONSTRAINT) return core::Status::Error(core::StatusCode::AlreadyExists, "Bucket already exists", name);
    status = sqlite_status(db, result, "Could not create bucket"); if (!status.ok()) return status;
    return BucketRecord{static_cast<BucketId>(sqlite3_last_insert_rowid(db)), std::move(name)};
#else
    return status;
#endif
}

core::Result<BucketRecord> SqliteMetadataStore::find_bucket(std::string_view name) {
    std::lock_guard<std::mutex> lock(mutex_); auto status = ensure_open_locked(); if (!status.ok()) return status;
#if defined(LOCALHIVE_HAS_SQLITE3)
    auto* db = static_cast<sqlite3*>(database_); Statement statement(db, "SELECT id,name FROM buckets WHERE name=?1;");
    if (statement.result() != SQLITE_OK) return sqlite_status(db, statement.result(), "Could not prepare bucket query");
    sqlite3_bind_text(statement.get(), 1, name.data(), static_cast<int>(name.size()), SQLITE_TRANSIENT);
    const int result = sqlite3_step(statement.get());
    if (result == SQLITE_DONE) return core::Status::Error(core::StatusCode::NotFound, "Bucket does not exist", std::string(name));
    status = sqlite_status(db, result, "Could not query bucket"); if (!status.ok()) return status;
    return BucketRecord{static_cast<BucketId>(sqlite3_column_int64(statement.get(), 0)), reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 1))};
#else
    return status;
#endif
}

core::Result<ObjectRecord> SqliteMetadataStore::find_or_create_object(BucketId bucket_id, std::string_view key) {
    auto normalized = normalize_object_key(key); if (!normalized.ok()) return normalized.status();
    std::lock_guard<std::mutex> lock(mutex_); auto status = ensure_open_locked(); if (!status.ok()) return status;
#if defined(LOCALHIVE_HAS_SQLITE3)
    auto* db = static_cast<sqlite3*>(database_);
    Statement insert(db, "INSERT INTO objects(bucket_id,key) VALUES(?1,?2) ON CONFLICT(bucket_id,key) DO NOTHING;");
    if (insert.result() != SQLITE_OK) return sqlite_status(db, insert.result(), "Could not prepare object insert");
    sqlite3_bind_int64(insert.get(), 1, static_cast<sqlite3_int64>(bucket_id)); sqlite3_bind_text(insert.get(), 2, normalized.value().c_str(), -1, SQLITE_TRANSIENT);
    status = sqlite_status(db, sqlite3_step(insert.get()), "Could not create object"); if (!status.ok()) return status;
    Statement select(db, "SELECT id,active_gen_id FROM objects WHERE bucket_id=?1 AND key=?2;");
    if (select.result() != SQLITE_OK) return sqlite_status(db, select.result(), "Could not prepare object query");
    sqlite3_bind_int64(select.get(), 1, static_cast<sqlite3_int64>(bucket_id)); sqlite3_bind_text(select.get(), 2, normalized.value().c_str(), -1, SQLITE_TRANSIENT);
    status = sqlite_status(db, sqlite3_step(select.get()), "Could not query object"); if (!status.ok()) return status;
    ObjectRecord record{static_cast<ObjectId>(sqlite3_column_int64(select.get(), 0)), bucket_id, normalized.value(), std::nullopt};
    if (sqlite3_column_type(select.get(), 1) != SQLITE_NULL) record.active_generation_id = static_cast<GenerationId>(sqlite3_column_int64(select.get(), 1));
    return record;
#else
    return status;
#endif
}

core::Status SqliteMetadataStore::create_generation(const GenerationRecord& generation) {
    std::lock_guard<std::mutex> lock(mutex_); auto status = ensure_open_locked(); if (!status.ok()) return status;
#if defined(LOCALHIVE_HAS_SQLITE3)
    auto* db = static_cast<sqlite3*>(database_); Statement statement(db, "INSERT INTO generations(gen_id,object_id,state,content_length,checksum_sha256) VALUES(?1,?2,?3,?4,?5);");
    if (statement.result() != SQLITE_OK) return sqlite_status(db, statement.result(), "Could not prepare generation insert");
    sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(generation.identity.generation_id)); sqlite3_bind_int64(statement.get(), 2, static_cast<sqlite3_int64>(generation.identity.object_id));
    sqlite3_bind_text(statement.get(), 3, state_name(generation.state), -1, SQLITE_STATIC); sqlite3_bind_int64(statement.get(), 4, static_cast<sqlite3_int64>(generation.content_length)); sqlite3_bind_text(statement.get(), 5, generation.checksum_sha256.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite_status(db, sqlite3_step(statement.get()), "Could not create generation");
#else
    return status;
#endif
}

core::Status SqliteMetadataStore::set_generation_state(GenerationId generation_id,
                                                        GenerationState state) {
    std::lock_guard<std::mutex> lock(mutex_); auto status = ensure_open_locked(); if (!status.ok()) return status;
#if defined(LOCALHIVE_HAS_SQLITE3)
    auto* db = static_cast<sqlite3*>(database_);
    Statement statement(db, "UPDATE generations SET state=?1 WHERE gen_id=?2;");
    if (statement.result() != SQLITE_OK) return sqlite_status(db, statement.result(), "Could not prepare generation state update");
    sqlite3_bind_text(statement.get(), 1, state_name(state), -1, SQLITE_STATIC);
    sqlite3_bind_int64(statement.get(), 2, static_cast<sqlite3_int64>(generation_id));
    status = sqlite_status(db, sqlite3_step(statement.get()), "Could not update generation state");
    if (!status.ok()) return status;
    if (sqlite3_changes(db) != 1) return core::Status::Error(core::StatusCode::NotFound, "Generation does not exist");
    return core::Status::Ok();
#else
    return status;
#endif
}

core::Status SqliteMetadataStore::stage_generation(GenerationId generation_id,
                                                    uint64_t content_length,
                                                    std::string_view checksum_sha256) {
    std::lock_guard<std::mutex> lock(mutex_); auto status = ensure_open_locked(); if (!status.ok()) return status;
#if defined(LOCALHIVE_HAS_SQLITE3)
    auto* db = static_cast<sqlite3*>(database_);
    Statement statement(db, "UPDATE generations SET state='staged',content_length=?1,checksum_sha256=?2 WHERE gen_id=?3 AND state='receiving';");
    if (statement.result() != SQLITE_OK) return sqlite_status(db, statement.result(), "Could not prepare generation staging update");
    sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(content_length));
    sqlite3_bind_text(statement.get(), 2, checksum_sha256.data(), static_cast<int>(checksum_sha256.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement.get(), 3, static_cast<sqlite3_int64>(generation_id));
    status = sqlite_status(db, sqlite3_step(statement.get()), "Could not stage generation");
    if (!status.ok()) return status;
    if (sqlite3_changes(db) != 1) return core::Status::Error(core::StatusCode::FailedPrecondition, "Generation is not receiving");
    return core::Status::Ok();
#else
    return status;
#endif
}

core::Status SqliteMetadataStore::publish_generation(GenerationId generation_id) {
    std::lock_guard<std::mutex> lock(mutex_); auto status = ensure_open_locked(); if (!status.ok()) return status;
#if defined(LOCALHIVE_HAS_SQLITE3)
    auto* db = static_cast<sqlite3*>(database_); status = execute(db, "BEGIN IMMEDIATE;"); if (!status.ok()) return status;
    Statement lookup(db, "SELECT object_id FROM generations WHERE gen_id=?1 AND state IN ('staged','prepared');");
    if (lookup.result() != SQLITE_OK) { execute(db, "ROLLBACK;"); return sqlite_status(db, lookup.result(), "Could not prepare generation lookup"); }
    sqlite3_bind_int64(lookup.get(), 1, static_cast<sqlite3_int64>(generation_id));
    if (sqlite3_step(lookup.get()) != SQLITE_ROW) { execute(db, "ROLLBACK;"); return core::Status::Error(core::StatusCode::FailedPrecondition, "Generation is not publishable"); }
    const auto object_id = sqlite3_column_int64(lookup.get(), 0);
    Statement retire(db, "UPDATE generations SET state='obsolete' WHERE gen_id=(SELECT active_gen_id FROM objects WHERE id=?1);");
    Statement publish(db, "UPDATE generations SET state='published' WHERE gen_id=?1;");
    Statement activate(db, "UPDATE objects SET active_gen_id=?1 WHERE id=?2;");
    if (retire.result() != SQLITE_OK || publish.result() != SQLITE_OK || activate.result() != SQLITE_OK) { execute(db, "ROLLBACK;"); return core::Status::Error(core::StatusCode::Internal, "Could not prepare publication transaction"); }
    sqlite3_bind_int64(retire.get(), 1, object_id); sqlite3_bind_int64(publish.get(), 1, static_cast<sqlite3_int64>(generation_id)); sqlite3_bind_int64(activate.get(), 1, static_cast<sqlite3_int64>(generation_id)); sqlite3_bind_int64(activate.get(), 2, object_id);
    if (sqlite3_step(retire.get()) != SQLITE_DONE || sqlite3_step(publish.get()) != SQLITE_DONE || sqlite3_step(activate.get()) != SQLITE_DONE) { execute(db, "ROLLBACK;"); return core::Status::Error(core::StatusCode::Internal, "Could not publish generation", sqlite3_errmsg(db)); }
    return execute(db, "COMMIT;");
#else
    return status;
#endif
}

core::Result<GenerationRecord> SqliteMetadataStore::active_generation(BucketId bucket_id, std::string_view key) {
    auto normalized = normalize_object_key(key); if (!normalized.ok()) return normalized.status();
    std::lock_guard<std::mutex> lock(mutex_); auto status = ensure_open_locked(); if (!status.ok()) return status;
#if defined(LOCALHIVE_HAS_SQLITE3)
    auto* db = static_cast<sqlite3*>(database_); Statement statement(db, "SELECT o.id,g.gen_id,g.state,g.content_length,g.checksum_sha256 FROM objects o JOIN generations g ON g.gen_id=o.active_gen_id WHERE o.bucket_id=?1 AND o.key=?2;");
    if (statement.result() != SQLITE_OK) return sqlite_status(db, statement.result(), "Could not prepare active generation query");
    sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(bucket_id)); sqlite3_bind_text(statement.get(), 2, normalized.value().c_str(), -1, SQLITE_TRANSIENT);
    const int result = sqlite3_step(statement.get()); if (result == SQLITE_DONE) return core::Status::Error(core::StatusCode::NotFound, "Object has no active generation");
    status = sqlite_status(db, result, "Could not query active generation"); if (!status.ok()) return status;
    return GenerationRecord{{bucket_id, static_cast<ObjectId>(sqlite3_column_int64(statement.get(), 0)), static_cast<GenerationId>(sqlite3_column_int64(statement.get(), 1))}, GenerationState::Published, static_cast<uint64_t>(sqlite3_column_int64(statement.get(), 3)), reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 4))};
#else
    return status;
#endif
}

core::Status SqliteMetadataStore::delete_active_object(BucketId bucket_id,
                                                        std::string_view key) {
    auto normalized = normalize_object_key(key);
    if (!normalized.ok()) return normalized.status();
    std::lock_guard<std::mutex> lock(mutex_);
    auto status = ensure_open_locked();
    if (!status.ok()) return status;
#if defined(LOCALHIVE_HAS_SQLITE3)
    auto* db = static_cast<sqlite3*>(database_);
    status = execute(db, "BEGIN IMMEDIATE;");
    if (!status.ok()) return status;

    sqlite3_int64 generation_id = 0;
    {
        Statement lookup(db, "SELECT active_gen_id FROM objects WHERE bucket_id=?1 AND key=?2;");
        if (lookup.result() != SQLITE_OK) {
            execute(db, "ROLLBACK;");
            return sqlite_status(db, lookup.result(), "Could not prepare object deletion lookup");
        }
        sqlite3_bind_int64(lookup.get(), 1, static_cast<sqlite3_int64>(bucket_id));
        sqlite3_bind_text(lookup.get(), 2, normalized.value().c_str(), -1, SQLITE_TRANSIENT);
        const int result = sqlite3_step(lookup.get());
        if (result == SQLITE_DONE) {
            execute(db, "ROLLBACK;");
            return core::Status::Error(core::StatusCode::NotFound,
                                       "Object has no active generation");
        }
        status = sqlite_status(db, result, "Could not find active object for deletion");
        if (!status.ok()) {
            execute(db, "ROLLBACK;");
            return status;
        }
        if (sqlite3_column_type(lookup.get(), 0) == SQLITE_NULL) {
            execute(db, "ROLLBACK;");
            return core::Status::Error(core::StatusCode::NotFound,
                                       "Object has no active generation");
        }
        generation_id = sqlite3_column_int64(lookup.get(), 0);
    }

    Statement tombstone(db,
        "UPDATE generations SET state='tombstone' WHERE gen_id=?1 "
        "AND state IN ('published','committed');");
    if (tombstone.result() != SQLITE_OK) {
        execute(db, "ROLLBACK;");
        return sqlite_status(db, tombstone.result(), "Could not prepare object tombstone");
    }
    sqlite3_bind_int64(tombstone.get(), 1, generation_id);
    status = sqlite_status(db, sqlite3_step(tombstone.get()), "Could not tombstone object generation");
    if (!status.ok() || sqlite3_changes(db) != 1) {
        execute(db, "ROLLBACK;");
        return status.ok()
            ? core::Status::Error(core::StatusCode::FailedPrecondition,
                                  "Active generation cannot be tombstoned")
            : status;
    }

    Statement deactivate(db,
        "UPDATE objects SET active_gen_id=NULL WHERE bucket_id=?1 AND key=?2 AND active_gen_id=?3;");
    if (deactivate.result() != SQLITE_OK) {
        execute(db, "ROLLBACK;");
        return sqlite_status(db, deactivate.result(), "Could not prepare active object removal");
    }
    sqlite3_bind_int64(deactivate.get(), 1, static_cast<sqlite3_int64>(bucket_id));
    sqlite3_bind_text(deactivate.get(), 2, normalized.value().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(deactivate.get(), 3, generation_id);
    status = sqlite_status(db, sqlite3_step(deactivate.get()), "Could not remove active object pointer");
    if (!status.ok() || sqlite3_changes(db) != 1) {
        execute(db, "ROLLBACK;");
        return status.ok()
            ? core::Status::Error(core::StatusCode::Aborted,
                                  "Active object changed during deletion")
            : status;
    }
    return execute(db, "COMMIT;");
#else
    return status;
#endif
}

core::Result<std::vector<RecoveryCandidate>> SqliteMetadataStore::recovery_candidates() {
    std::lock_guard<std::mutex> lock(mutex_); auto status = ensure_open_locked(); if (!status.ok()) return status;
#if defined(LOCALHIVE_HAS_SQLITE3)
    auto* db = static_cast<sqlite3*>(database_);
    Statement statement(db, "SELECT o.bucket_id,g.object_id,g.gen_id,g.state FROM generations g JOIN objects o ON o.id=g.object_id WHERE g.state IN ('receiving','staged','prepared','published');");
    if (statement.result() != SQLITE_OK) return sqlite_status(db, statement.result(), "Could not prepare recovery query");
    std::vector<RecoveryCandidate> candidates;
    while (true) {
        const int result = sqlite3_step(statement.get());
        if (result == SQLITE_DONE) break;
        status = sqlite_status(db, result, "Could not read recovery candidates"); if (!status.ok()) return status;
        auto state = parse_state(reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 3)));
        if (!state.ok()) return state.status();
        candidates.push_back({{static_cast<BucketId>(sqlite3_column_int64(statement.get(), 0)),
                               static_cast<ObjectId>(sqlite3_column_int64(statement.get(), 1)),
                               static_cast<GenerationId>(sqlite3_column_int64(statement.get(), 2))}, state.value()});
    }
    return candidates;
#else
    return status;
#endif
}

core::Result<std::vector<RecoveryCandidate>> SqliteMetadataStore::reclaimable_generations() {
    std::lock_guard<std::mutex> lock(mutex_); auto status = ensure_open_locked(); if (!status.ok()) return status;
#if defined(LOCALHIVE_HAS_SQLITE3)
    auto* db = static_cast<sqlite3*>(database_);
    Statement statement(db, "SELECT o.bucket_id,g.object_id,g.gen_id,g.state FROM generations g JOIN objects o ON o.id=g.object_id WHERE g.state IN ('obsolete','tombstone');");
    if (statement.result() != SQLITE_OK) return sqlite_status(db, statement.result(), "Could not prepare garbage collection query");
    std::vector<RecoveryCandidate> candidates;
    while (true) {
        const int result = sqlite3_step(statement.get());
        if (result == SQLITE_DONE) break;
        status = sqlite_status(db, result, "Could not read reclaimable generations"); if (!status.ok()) return status;
        auto state = parse_state(reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 3)));
        if (!state.ok()) return state.status();
        candidates.push_back({{static_cast<BucketId>(sqlite3_column_int64(statement.get(), 0)),
                               static_cast<ObjectId>(sqlite3_column_int64(statement.get(), 1)),
                               static_cast<GenerationId>(sqlite3_column_int64(statement.get(), 2))}, state.value()});
    }
    return candidates;
#else
    return status;
#endif
}

core::Status SqliteMetadataStore::remove_generation(GenerationId generation_id) {
    std::lock_guard<std::mutex> lock(mutex_); auto status = ensure_open_locked(); if (!status.ok()) return status;
#if defined(LOCALHIVE_HAS_SQLITE3)
    auto* db = static_cast<sqlite3*>(database_);
    Statement statement(db, "DELETE FROM generations WHERE gen_id=?1 AND gen_id NOT IN (SELECT active_gen_id FROM objects WHERE active_gen_id IS NOT NULL);");
    if (statement.result() != SQLITE_OK) return sqlite_status(db, statement.result(), "Could not prepare generation deletion");
    sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(generation_id));
    status = sqlite_status(db, sqlite3_step(statement.get()), "Could not delete generation"); if (!status.ok()) return status;
    if (sqlite3_changes(db) != 1) return core::Status::Error(core::StatusCode::FailedPrecondition, "Generation is active or does not exist");
    return core::Status::Ok();
#else
    return status;
#endif
}

} // namespace localhive::storage
