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

bool MetadataManager::removeMetadata(const std::string &key) {
    std::string sql = "DELETE FROM Metadata WHERE Key = ?;";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return true;
    }

    std::cerr << "Error removing metadata: " << sqlite3_errmsg(db) << std::endl;
    sqlite3_finalize(stmt);
    return false;
}

bool MetadataManager::getMetadata(const std::string &key, size_t &size, std::string &creationDate, std::string &lastModified, std::string &contentType) {
    std::string sql = "SELECT Size, CreationDate, LastModifiedDate, ContentType FROM Metadata WHERE Key = ?;";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        size = sqlite3_column_int64(stmt, 0);
        creationDate = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        lastModified = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        contentType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        sqlite3_finalize(stmt);
        return true;
    }

    sqlite3_finalize(stmt);
    return false;
}

void MetadataManager::listAllMetadata() {
    std::string sql = "SELECT Key, Size, CreationDate, LastModifiedDate, ContentType FROM Metadata;";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

    std::cout << "\n=== Metadata Listing ===" << std::endl;
    std::cout << "Key\t\tSize\tCreation Date\t\tContent Type" << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::cout << sqlite3_column_text(stmt, 0) << "\t";
        std::cout << sqlite3_column_int64(stmt, 1) << "\t";
        std::cout << sqlite3_column_text(stmt, 2) << "\t";
        std::cout << sqlite3_column_text(stmt, 4) << std::endl;
    }

    sqlite3_finalize(stmt);
}
