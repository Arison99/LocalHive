// ===========================
// File: /lib/metadata_manager.cpp
// ===========================
#include "metadata_manager.h"
#include <sqlite3.h>
#include <iostream>

MetadataManager::MetadataManager(const std::string &dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db)) {
        std::cerr << "Error opening SQLite database: " << sqlite3_errmsg(db) << std::endl;
    } else {
        std::string createTableSQL = "CREATE TABLE IF NOT EXISTS Metadata ("
                                     "Key TEXT PRIMARY KEY, "
                                     "Size INTEGER, "
                                     "CreationDate TEXT, "
                                     "LastModifiedDate TEXT, "
                                     "ContentType TEXT);";
        sqlite3_exec(db, createTableSQL.c_str(), nullptr, nullptr, nullptr);
    }
}

MetadataManager::~MetadataManager() {
    sqlite3_close(db);
}

bool MetadataManager::addMetadata(const std::string &key, size_t size, const std::string &creationDate, const std::string &contentType) {
    std::string sql = "INSERT INTO Metadata (Key, Size, CreationDate, LastModifiedDate, ContentType) VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, size);
    sqlite3_bind_text(stmt, 3, creationDate.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, creationDate.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, contentType.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return true;
    }

    std::cerr << "Error adding metadata: " << sqlite3_errmsg(db) << std::endl;
    sqlite3_finalize(stmt);
    return false;
}
