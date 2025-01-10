
// ===========================
// File: /include/routes/upload.h
// ===========================
#ifndef ROUTES_UPLOAD_H
#define ROUTES_UPLOAD_H

#include <boost/asio.hpp>

void handleUpload(boost::asio::ip::tcp::socket &socket);

#endif // ROUTES_UPLOAD_H