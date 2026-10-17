// ===========================
// File: /tests/metadata_test.cpp
// ===========================
#include "../include/metadata_manager.h"
#include <iostream>
#include <filesystem>
#include <cassert>

namespace fs = std::filesystem;

void testAddMetadata() {
    std::cout << "Testing metadata addition..." << std::endl;
    
    MetadataManager manager("test_metadata.db");
    
    bool result = manager.addMetadata("bucket1/file1.txt", 1024, "2025-11-18", "text/plain");
    assert(result == true);
    
    result = manager.addMetadata("bucket1/image.jpg", 2048, "2025-11-18", "image/jpeg");
    assert(result == true);
    
    std::cout << "✓ Metadata addition tests passed!" << std::endl;
}

void testGetMetadata() {
    std::cout << "Testing metadata retrieval..." << std::endl;
    
    MetadataManager manager("test_metadata.db");
    
    size_t size;
    std::string creationDate, lastModified, contentType;
    
    bool result = manager.getMetadata("bucket1/file1.txt", size, creationDate, lastModified, contentType);
    assert(result == true);
    assert(size == 1024);
    assert(creationDate == "2025-11-18");
    assert(contentType == "text/plain");
    
    // Test non-existent metadata
    result = manager.getMetadata("nonexistent/file.txt", size, creationDate, lastModified, contentType);
    assert(result == false);
    
    std::cout << "✓ Metadata retrieval tests passed!" << std::endl;
}

void testListMetadata() {
    std::cout << "Testing metadata listing..." << std::endl;
    
    MetadataManager manager("test_metadata.db");
    
    // Add some more metadata for listing
    manager.addMetadata("bucket2/doc.pdf", 4096, "2025-11-18", "application/pdf");
    
    std::cout << "Listing all metadata:" << std::endl;
    manager.listAllMetadata();
    
    std::cout << "✓ Metadata listing test completed!" << std::endl;
}

void testRemoveMetadata() {
    std::cout << "Testing metadata removal..." << std::endl;
    
    MetadataManager manager("test_metadata.db");
    
    bool result = manager.removeMetadata("bucket1/file1.txt");
    assert(result == true);
    
    // Verify it's removed by trying to get it
    size_t size;
    std::string creationDate, lastModified, contentType;
    result = manager.getMetadata("bucket1/file1.txt", size, creationDate, lastModified, contentType);
    assert(result == false);
    
    std::cout << "✓ Metadata removal tests passed!" << std::endl;
}

void cleanup() {
    std::cout << "Cleaning up test database..." << std::endl;
    if (fs::exists("test_metadata.db")) {
        fs::remove("test_metadata.db");
    }
    std::cout << "✓ Cleanup completed!" << std::endl;
}

int main() {
    std::cout << "=== Running Metadata Manager Tests ===" << std::endl;
    
    try {
        testAddMetadata();
        testGetMetadata();
        testListMetadata();
        testRemoveMetadata();
        cleanup();
        
        std::cout << "\\n✓ All metadata tests passed!" << std::endl;
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}
