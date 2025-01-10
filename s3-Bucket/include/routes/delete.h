// ===========================
// File: /include/routes/delete.h
// ===========================
#ifndef ROUTES_DELETE_H
#define ROUTES_DELETE_H

#include <boost/asio.hpp>

void handleDelete(boost::asio::ip::tcp::socket &socket);

#endif // ROUTES_DELETE_H