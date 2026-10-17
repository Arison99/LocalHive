
// ===========================
// File: /server/server.cpp
// ===========================
#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <thread>
#include "routes/upload.h"
#include "routes/download.h"
#include "routes/delete.h"
#include "bucket_manager.h"

using namespace boost::asio;

void handleRequest(ip::tcp::socket &socket) {
    try {
        boost::asio::streambuf buffer;
        boost::system::error_code error;
        
        // Read request line
        boost::asio::read_until(socket, buffer, "\r\n", error);
        if (error) {
            std::cerr << "Error reading request: " << error.message() << std::endl;
            return;
        }
        
        std::istream stream(&buffer);
        std::string requestLine;
        std::getline(stream, requestLine);
        
        std::cout << "Request: " << requestLine << std::endl;

        if (requestLine.find("POST /upload") != std::string::npos) {
            handleUpload(socket);
        } else if (requestLine.find("GET /download") != std::string::npos) {
            handleDownload(socket);
        } else if (requestLine.find("DELETE /delete") != std::string::npos) {
            handleDelete(socket);
        } else if (requestLine.find("GET /buckets") != std::string::npos) {
            // List buckets endpoint
            BucketManager manager("buckets");
            std::string response = "HTTP/1.1 200 OK\r\n"
                                 "Content-Type: text/plain\r\n"
                                 "Content-Length: 19\r\n\r\n"
                                 "Available buckets:\n";
            boost::asio::write(socket, boost::asio::buffer(response));
            manager.listBuckets();
        } else if (requestLine.find("GET /health") != std::string::npos) {
            // Health check endpoint
            std::string response = "HTTP/1.1 200 OK\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: 25\r\n\r\n"
                                 "{\"status\": \"Server is running\"}";
            boost::asio::write(socket, boost::asio::buffer(response));
        } else {
            std::string response = "HTTP/1.1 404 Not Found\r\n"
                                 "Content-Type: text/plain\r\n"
                                 "Content-Length: 13\r\n\r\n"
                                 "Page not found";
            boost::asio::write(socket, boost::asio::buffer(response));
        }
    } catch (const std::exception &e) {
        std::cerr << "Error handling request: " << e.what() << std::endl;
    }
}

void handleClient(ip::tcp::socket socket) {
    handleRequest(socket);
}

int main() {
    try {
        io_context ioContext;
        ip::tcp::acceptor acceptor(ioContext, ip::tcp::endpoint(ip::tcp::v4(), 8080));
        
        std::cout << "LocalHive S3 Server starting on port 8080..." << std::endl;
        std::cout << "Available endpoints:" << std::endl;
        std::cout << "  POST /upload?bucket=NAME&key=FILENAME" << std::endl;
        std::cout << "  GET  /download?bucket=NAME&key=FILENAME" << std::endl;
        std::cout << "  DELETE /delete?bucket=NAME&key=FILENAME" << std::endl;
        std::cout << "  GET  /buckets" << std::endl;
        std::cout << "  GET  /health" << std::endl;
        
        // Ensure buckets directory exists
        BucketManager manager("buckets");
        
        while (true) {
            ip::tcp::socket socket(ioContext);
            acceptor.accept(socket);
            
            // Handle each client in a separate thread
            std::thread clientThread(handleClient, std::move(socket));
            clientThread.detach();
        }
    } catch (const std::exception &e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
}
