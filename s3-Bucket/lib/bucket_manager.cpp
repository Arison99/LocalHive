// ===========================
// File: /lib/bucket_manager.cpp
// ===========================
#include "bucket_manager.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

BucketManager::BucketManager(const std::string &baseDir) : baseDirectory(baseDir) {
    if (!fs::exists(baseDir)) {
        fs::create_directories(baseDir);
    }
}

bool BucketManager::createBucket(const std::string &bucketName) {
    std::string bucketPath = baseDirectory + "/" + bucketName;
    if (!fs::exists(bucketPath)) {
        fs::create_directories(bucketPath + "/objects");
        std::ofstream configFile(bucketPath + "/config.json");
        configFile << "{\"bucketName\": \"" << bucketName << "\"}";
        configFile.close();
        return true;
    }
    return false;
}

bool BucketManager::deleteBucket(const std::string &bucketName) {
    std::string bucketPath = baseDirectory + "/" + bucketName;
    if (fs::exists(bucketPath)) {
        fs::remove_all(bucketPath);
        return true;
    }
    return false;
}

void BucketManager::listBuckets() {
    for (const auto &entry : fs::directory_iterator(baseDirectory)) {
        if (fs::is_directory(entry.path())) {
            std::cout << entry.path().filename() << std::endl;
        }
    }
}
