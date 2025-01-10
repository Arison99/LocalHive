// ===========================
// File: /include/metadata_manager.h
// ===========================
#ifndef METADATA_MANAGER_H
#define METADATA_MANAGER_H

#include <string>
#include <sqlite3.h>

class MetadataManager {
public:
    explicit MetadataManager(const std::string &dbPath);
    ~MetadataManager();

    bool addMetadata(const std::string &key, size_t size, const std::string &creationDate, const std::string &contentType);

private:
    sqlite3 *db;
};

#endif // METADATA_MANAGER_H
