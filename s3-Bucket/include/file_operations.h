// ===========================
// File: /include/file_operations.h
// ===========================
#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include <string>

class FileOperations {
public:
    static bool uploadFile(const std::string &filePath, const std::string &destPath);
    static bool deleteFile(const std::string &filePath);
    static std::string readFile(const std::string &filePath);
};

#endif // FILE_OPERATIONS_H