// ===========================
// File: /server/routes/upload.cpp
// ===========================
#include "routes/upload.h"
#include "bucket_manager.h"
#include "file_operations.h"
#include "metadata_manager.h"
#include <boost/asio.hpp>
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

void handleUpload(boost::asio::ip::tcp::socket &socket) {
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
        
        // Read headers to get content length
        std::string header_line;
        size_t content_length = 0;
        while (std::getline(request_stream, header_line) && header_line != "\r") {
            if (header_line.substr(0, 16) == "Content-Length: ") {
                content_length = std::stoul(header_line.substr(16));
            }
        }
        
        // Read the file content
        std::vector<char> file_content(content_length);
        boost::asio::read(socket, boost::asio::buffer(file_content));
        
        // Ensure bucket exists
        BucketManager bucket_manager("buckets");
        if (!fs::exists("buckets/" + bucket_name)) {
            bucket_manager.createBucket(bucket_name);
        }
        
        // Save the file
        std::string file_path = "buckets/" + bucket_name + "/objects/" + object_key;
        std::ofstream output_file(file_path, std::ios::binary);
        if (!output_file) {
            std::string response = "HTTP/1.1 500 Internal Server Error\r\n"
                                 "Content-Type: text/plain\r\n"
                                 "Content-Length: 24\r\n\r\n"
                                 "Failed to create file";
            boost::asio::write(socket, boost::asio::buffer(response));
            return;
        }
        
        output_file.write(file_content.data(), content_length);
        output_file.close();
        
        // Add metadata
        MetadataManager metadata_db("buckets/metadata.db");
        std::string creation_date = "2025-11-18"; // You can implement proper date/time
        metadata_db.addMetadata(bucket_name + "/" + object_key, content_length, creation_date, "application/octet-stream");
        
        // Send success response
        std::string response = "HTTP/1.1 200 OK\r\n"
                             "Content-Type: application/json\r\n"
                             "Content-Length: 32\r\n\r\n"
                             "{\"status\": \"File uploaded successfully\"}";
        boost::asio::write(socket, boost::asio::buffer(response));
        
    } catch (const std::exception &e) {
        std::string response = "HTTP/1.1 500 Internal Server Error\r\n"
                             "Content-Type: text/plain\r\n"
                             "Content-Length: 21\r\n\r\n"
                             "Internal server error";
        boost::asio::write(socket, boost::asio::buffer(response));
        std::cerr << "Upload error: " << e.what() << std::endl;
    }
}
