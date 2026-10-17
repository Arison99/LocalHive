// ===========================
// File: /tests/file_test.cpp
// ===========================
#include "../include/file_operations.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cassert>

namespace fs = std::filesystem;

void setupTestFiles() {
    // Create test directory
    fs::create_directories("test_files");
    
    // Create a test file
    std::ofstream testFile("test_files/source.txt");
    testFile << "This is a test file content for upload testing.";
    testFile.close();
}

void testUploadFile() {
    std::cout << "Testing file upload..." << std::endl;
    
    // Create destination directory
    fs::create_directories("test_files/destination");
    
    bool result = FileOperations::uploadFile("test_files/source.txt", "test_files/destination/uploaded.txt");
    assert(result == true);
    assert(fs::exists("test_files/destination/uploaded.txt"));
    
    // Verify content is the same
    std::string originalContent = FileOperations::readFile("test_files/source.txt");
    std::string uploadedContent = FileOperations::readFile("test_files/destination/uploaded.txt");
    assert(originalContent == uploadedContent);
    
    std::cout << "✓ File upload tests passed!" << std::endl;
}

void testDeleteFile() {
    std::cout << "Testing file deletion..." << std::endl;
    
    // Create a file to delete
    std::ofstream testFile("test_files/to_delete.txt");
    testFile << "This file will be deleted";
    testFile.close();
    
    assert(fs::exists("test_files/to_delete.txt"));
    
    bool result = FileOperations::deleteFile("test_files/to_delete.txt");
    assert(result == true);
    assert(!fs::exists("test_files/to_delete.txt"));
    
    // Try to delete non-existent file
    result = FileOperations::deleteFile("test_files/non_existent.txt");
    assert(result == false);
    
    std::cout << "✓ File deletion tests passed!" << std::endl;
}

void testReadFile() {
    std::cout << "Testing file reading..." << std::endl;
    
    // Create a file with known content
    std::ofstream testFile("test_files/read_test.txt");
    std::string expectedContent = "Hello, World!\\nThis is line 2.\\nEnd of file.";
    testFile << expectedContent;
    testFile.close();
    
    std::string actualContent = FileOperations::readFile("test_files/read_test.txt");
    assert(actualContent == expectedContent);
    
    // Test reading non-existent file
    std::string errorContent = FileOperations::readFile("test_files/non_existent.txt");
    assert(errorContent == "Error opening file");
    
    std::cout << "✓ File reading tests passed!" << std::endl;
}

void cleanup() {
    std::cout << "Cleaning up test files..." << std::endl;
    if (fs::exists("test_files")) {
        fs::remove_all("test_files");
    }
    std::cout << "✓ Cleanup completed!" << std::endl;
}

int main() {
    std::cout << "=== Running File Operations Tests ===" << std::endl;
    
    try {
        setupTestFiles();
        testUploadFile();
        testDeleteFile();
        testReadFile();
        cleanup();
        
        std::cout << "\\n✓ All file operation tests passed!" << std::endl;
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}
