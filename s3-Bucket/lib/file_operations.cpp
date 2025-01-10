
// ===========================
// File: /lib/file_operations.cpp
// ===========================
#include "file_operations.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

bool FileOperations::uploadFile(const std::string &filePath, const std::string &destPath) {
    try {
        fs::copy(filePath, destPath, fs::copy_options::overwrite_existing);
        return true;
    } catch (const fs::filesystem_error &e) {
        std::cerr << "Error uploading file: " << e.what() << std::endl;
        return false;
    }
}

bool FileOperations::deleteFile(const std::string &filePath) {
    try {
        return fs::remove(filePath);
    } catch (const fs::filesystem_error &e) {
        std::cerr << "Error deleting file: " << e.what() << std::endl;
        return false;
    }
}

std::string FileOperations::readFile(const std::string &filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return "Error opening file";
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content;
}


