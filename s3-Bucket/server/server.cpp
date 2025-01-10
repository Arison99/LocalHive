
// ===========================
// File: /server/server.cpp
// ===========================
#include <boost/asio.hpp>
#include <iostream>
#include "routes/upload.h"
#include "routes/download.h"
#include "routes/delete.h"

using namespace boost::asio;

void handleRequest(ip::tcp::socket &socket) {
    boost::asio::streambuf buffer;
    boost::asio::read_until(socket, buffer, "\r\n");
    std::istream stream(&buffer);
    std::string requestLine;
    std::getline(stream, requestLine);

    if (requestLine.find("POST /upload") != std::string::npos) {
        handleUpload(socket);
    } else if (requestLine.find("GET /download") != std::string::npos) {
        handleDownload(socket);
    } else if (requestLine.find("DELETE /delete") != std::string::npos) {
        handleDelete(socket);
    } else {
        std::string response = "HTTP/1.1 404 Not Found\r\n\r\n";
        boost::asio::write(socket, boost::asio::buffer(response));
    }
}

int main() {
    io_context ioContext;
    ip::tcp::acceptor acceptor(ioContext, ip::tcp::endpoint(ip::tcp::v4(), 8080));

    while (true) {
        ip::tcp::socket socket(ioContext);
        acceptor.accept(socket);
        handleRequest(socket);
    }
}
