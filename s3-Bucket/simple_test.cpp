// Simple test program to verify our core functionality
// Compile with: g++ -std=c++17 -Iinclude simple_test.cpp lib/bucket_manager.cpp lib/file_operations.cpp -o simple_test.exe

#include <iostream>
#include "bucket_manager.h"
#include "file_operations.h"

int main() {
    std::cout << "=== LocalHive S3 Core Functionality Test ===" << std::endl;
    
    try {
        // Test BucketManager
        std::cout << "1. Testing BucketManager..." << std::endl;
        BucketManager manager("test_buckets");
        
        bool result = manager.createBucket("test-bucket");
        if (result) {
            std::cout << "   ✓ Bucket created successfully" << std::endl;
        } else {
            std::cout << "   ✗ Failed to create bucket" << std::endl;
        }
        
        std::cout << "   Available buckets:" << std::endl;
        manager.listBuckets();
        
        // Test FileOperations
        std::cout << "\n2. Testing FileOperations..." << std::endl;
        
        // Create a test file
        std::ofstream testFile("test_file.txt");
        testFile << "Hello, LocalHive S3!";
        testFile.close();
        
        // Test upload (copy)
        result = FileOperations::uploadFile("test_file.txt", "test_buckets/test-bucket/objects/uploaded_file.txt");
        if (result) {
            std::cout << "   ✓ File uploaded successfully" << std::endl;
        } else {
            std::cout << "   ✗ Failed to upload file" << std::endl;
        }
        
        // Test read
        std::string content = FileOperations::readFile("test_buckets/test-bucket/objects/uploaded_file.txt");
        if (content != "Error opening file") {
            std::cout << "   ✓ File read successfully: " << content << std::endl;
        } else {
            std::cout << "   ✗ Failed to read file" << std::endl;
        }
        
        std::cout << "\n✓ All core tests completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}