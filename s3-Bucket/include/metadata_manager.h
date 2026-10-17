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
    bool removeMetadata(const std::string &key);
    bool getMetadata(const std::string &key, size_t &size, std::string &creationDate, std::string &lastModified, std::string &contentType);
    void listAllMetadata();

private:
    sqlite3 *db;
};

#endif // METADATA_MANAGER_H
