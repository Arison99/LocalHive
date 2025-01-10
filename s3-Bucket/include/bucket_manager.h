// ===========================
// File: /include/bucket_manager.h
// ===========================
#ifndef BUCKET_MANAGER_H
#define BUCKET_MANAGER_H

#include <string>

class BucketManager {
public:
    explicit BucketManager(const std::string &baseDir);
    bool createBucket(const std::string &bucketName);
    bool deleteBucket(const std::string &bucketName);
    void listBuckets();

private:
    std::string baseDirectory;
};

#endif // BUCKET_MANAGER_H








// ===========================
// File Structure
// ===========================
// /s3-bucket-replica/
// ├── /include/               # Header files directory
// │   ├── bucket_manager.h    # Header for BucketManager
// │   ├── file_operations.h   # Header for FileOperations
// │   ├── metadata_manager.h  # Header for MetadataManager
// │   ├── routes/
// │   │   ├── upload.h        # Header for upload route
// │   │   ├── download.h      # Header for download route
// │   │   └── delete.h        # Header for delete route
// ├── /lib/                   # Source files directory
// ├── /server/                # Server logic directory
// ├── CMakeLists.txt          # Build configuration
// └── README.md               # Documentation
