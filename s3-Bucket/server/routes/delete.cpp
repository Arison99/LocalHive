// ===========================
// File: /server/routes/delete.cpp
// ===========================
#include "routes/delete.h"
#include "file_operations.h"
#include "metadata_manager.h"
#include <boost/asio.hpp>
#include <iostream>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

void handleDelete(boost::asio::ip::tcp::socket &socket) {
    try {
        // Read the HTTP request
        boost::asio::streambuf buffer;
        boost::asio::read_until(socket, buffer, "\r\n\r\n");
        
        std::istream request_stream(&buffer);
        std::string request_line;
        std::getline(request_stream, request_line);
        
        // Parse query parameters from URL
        std::string bucket_name, object_key;
        size_t query_pos = request_line.find('?');
        if (query_pos != std::string::npos) {
            std::string query_string = request_line.substr(query_pos + 1);
            std::string::size_type space_pos = query_string.find(' ');
            if (space_pos != std::string::npos) {
                query_string = query_string.substr(0, space_pos);
            }
            
            // Parse bucket and key parameters
            std::istringstream query_stream(query_string);
            std::string param;
            while (std::getline(query_stream, param, '&')) {
                size_t eq_pos = param.find('=');
                if (eq_pos != std::string::npos) {
                    std::string key = param.substr(0, eq_pos);
                    std::string value = param.substr(eq_pos + 1);
                    if (key == "bucket") {
                        bucket_name = value;
                    } else if (key == "key") {
                        object_key = value;
                    }
                }
            }
        }
        
        if (bucket_name.empty() || object_key.empty()) {
            std::string response = "HTTP/1.1 400 Bad Request\r\n"
                                 "Content-Type: text/plain\r\n"
                                 "Content-Length: 27\r\n\r\n"
                                 "Missing bucket or key param";
            boost::asio::write(socket, boost::asio::buffer(response));
            return;
        }
        
        // Check if file exists
        std::string file_path = "buckets/" + bucket_name + "/objects/" + object_key;
        if (!fs::exists(file_path)) {
            std::string response = "HTTP/1.1 404 Not Found\r\n"
                                 "Content-Type: text/plain\r\n"
                                 "Content-Length: 14\r\n\r\n"
                                 "File not found";
            boost::asio::write(socket, boost::asio::buffer(response));
            return;
        }
        
        // Delete the file
        if (FileOperations::deleteFile(file_path)) {
            // TODO: Remove metadata from database
            // MetadataManager metadata_db("buckets/metadata.db");
            // metadata_db.removeMetadata(bucket_name + "/" + object_key);
            
            std::string response = "HTTP/1.1 200 OK\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: 32\r\n\r\n"
                                 "{\"status\": \"File deleted successfully\"}";
            boost::asio::write(socket, boost::asio::buffer(response));
        } else {
            std::string response = "HTTP/1.1 500 Internal Server Error\r\n"
                                 "Content-Type: text/plain\r\n"
                                 "Content-Length: 21\r\n\r\n"
                                 "Failed to delete file";
            boost::asio::write(socket, boost::asio::buffer(response));
        }
        
    } catch (const std::exception &e) {
        std::string response = "HTTP/1.1 500 Internal Server Error\r\n"
                             "Content-Type: text/plain\r\n"
                             "Content-Length: 21\r\n\r\n"
                             "Internal server error";
        boost::asio::write(socket, boost::asio::buffer(response));
        std::cerr << "Delete error: " << e.what() << std::endl;
    }
}
