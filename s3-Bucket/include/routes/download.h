// ===========================
// File: /include/routes/download.h
// ===========================
#ifndef ROUTES_DOWNLOAD_H
#define ROUTES_DOWNLOAD_H

#include <boost/asio.hpp>

void handleDownload(boost::asio::ip::tcp::socket &socket);

#endif // ROUTES_DOWNLOAD_H
