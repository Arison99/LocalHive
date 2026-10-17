// ===========================
// File: /tests/bucket_test.cpp
// ===========================
#include "../include/bucket_manager.h"
#include <iostream>
#include <filesystem>
#include <cassert>

namespace fs = std::filesystem;

void testCreateBucket() {
    std::cout << "Testing bucket creation..." << std::endl;
    
    BucketManager manager("test_buckets");
    
    // Test creating a new bucket
    bool result = manager.createBucket("test-bucket-1");
    assert(result == true);
    assert(fs::exists("test_buckets/test-bucket-1"));
    assert(fs::exists("test_buckets/test-bucket-1/objects"));
    assert(fs::exists("test_buckets/test-bucket-1/config.json"));
    
    // Test creating duplicate bucket (should fail)
    result = manager.createBucket("test-bucket-1");
    assert(result == false);
    
    std::cout << "✓ Bucket creation tests passed!" << std::endl;
}

void testDeleteBucket() {
    std::cout << "Testing bucket deletion..." << std::endl;
    
    BucketManager manager("test_buckets");
    
    // Create a bucket first
    manager.createBucket("test-bucket-delete");
    assert(fs::exists("test_buckets/test-bucket-delete"));
    
    // Delete the bucket
    bool result = manager.deleteBucket("test-bucket-delete");
    assert(result == true);
    assert(!fs::exists("test_buckets/test-bucket-delete"));
    
    // Try to delete non-existent bucket
    result = manager.deleteBucket("non-existent-bucket");
    assert(result == false);
    
    std::cout << "✓ Bucket deletion tests passed!" << std::endl;
}

void testListBuckets() {
    std::cout << "Testing bucket listing..." << std::endl;
    
    BucketManager manager("test_buckets");
    
    // Create a few buckets
    manager.createBucket("bucket-1");
    manager.createBucket("bucket-2");
    manager.createBucket("bucket-3");
    
    std::cout << "Listing buckets:" << std::endl;
    manager.listBuckets();
    
    std::cout << "✓ Bucket listing test completed!" << std::endl;
}

void cleanup() {
    std::cout << "Cleaning up test files..." << std::endl;
    if (fs::exists("test_buckets")) {
        fs::remove_all("test_buckets");
    }
    std::cout << "✓ Cleanup completed!" << std::endl;
}

int main() {
    std::cout << "=== Running Bucket Manager Tests ===" << std::endl;
    
    try {
        testCreateBucket();
        testDeleteBucket();
        testListBuckets();
        cleanup();
        
        std::cout << "\\n✓ All bucket tests passed!" << std::endl;
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}
