/**
 * \file src/plugins/output/json/src/Server.cpp
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Server output
 *
 * Copyright (C) 2015-2018 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#include "Server.hpp"
#include <stdexcept>
#include <cstring>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>

/** How many pending connections queue will hold */
#define BACKLOG (10)

/**
 * \brief Class constructor
 *
 * \param[in] cfg Configuration
 * \param[in] ctx Instance context
 * Parse configuration, create and bind server's socket and create acceptor's
 * thread
 */
Server::Server(const struct cfg_server &cfg, ipx_ctx_t *ctx) : Output(cfg.name, ctx)
{
    std::string port = std::to_string(cfg.port);
    _non_blocking = !cfg.blocking;
    _acceptor = NULL;

    int serv_fd;
    int ret_val;

    // New socket configuration
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;       // Use IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     // Wildcard IP address
    hints.ai_protocol = IPPROTO_TCP; // TCP

    // Create new socket
    struct addrinfo *servinfo, *iter;
    if ((ret_val = getaddrinfo(NULL, port.c_str(), &hints, &servinfo)) != 0) {
        throw std::runtime_error("(Server output) Server initialization failed (" +
            std::string(gai_strerror(ret_val)) + ")");
    }

    for (iter = servinfo; iter != NULL; iter = iter->ai_next) {
        serv_fd = socket(iter->ai_family, iter->ai_socktype, iter->ai_protocol);
        if ((serv_fd) == -1) {
            continue;
        }

        int yes = 1;
        if (setsockopt(serv_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            close(serv_fd);
            continue;
        }

        if (bind(serv_fd, iter->ai_addr, iter->ai_addrlen) == -1) {
            // Failed to bind
            close(serv_fd);
            continue;
        }

        // Success
        break;
    }

    freeaddrinfo(servinfo);

    // Check if new socket is ready
    if (iter == NULL)  {
        throw std::runtime_error("(Server output) Server failed to bind to specified port.");
    }

    // Make socket passive
    if (listen(serv_fd, BACKLOG) == -1) {
        close(serv_fd);
        throw std::runtime_error("(Server output) Failed to initialize server (listen() failed).");
    }

    // Create thread
    _acceptor = new acceptor_t;
    _acceptor->ctx = ctx; // Only for log
    _acceptor->socket_fd = serv_fd;
    _acceptor->new_clients_ready = false;
    _acceptor->stop = false;

    if (pthread_mutex_init(&_acceptor->mutex, NULL) != 0) {
        delete _acceptor;
        close(serv_fd);
        throw std::runtime_error("(Server output) Mutex initialization failed!");
    }

    if (pthread_create(&_acceptor->thread, NULL, &Server::thread_accept,_acceptor) != 0) {
        pthread_mutex_destroy(&_acceptor->mutex);
        delete _acceptor;
        close(serv_fd);
        throw std::runtime_error("(Server output) Acceptor thread failed");
    }
}

/**
 * \brief Class destructor
 *
 * Close all sockets and stop and destroy the acceptor.
 */
Server::~Server()
{
    // Disconnect connected clients
    for (auto &client : _clients) {
        close(client.socket);
    }

    // Stop and destroy acceptor's thread
    if (_acceptor) {
        _acceptor->stop = true;
        pthread_join(_acceptor->thread, NULL);
        pthread_mutex_destroy(&_acceptor->mutex);

        close(_acceptor->socket_fd);

        for (auto &client : _acceptor->new_clients) {
            close(client.socket);
        }

        delete _acceptor;
    }
}

/**
 * \brief Acceptor's thread function
 *
 * Wait for new clients and accept them.
 * \param[in,out] context Acceptor's structure with configured server socket
 * \return Nothing
 */
void *
Server::thread_accept(void *context)
{
    acceptor_t *acc = (acceptor_t *) context;
    int ret_val;
    struct timeval tv;
    fd_set rfds;

    IPX_CTX_INFO(acc->ctx, "(Server output) Waiting for connections...", '\0');

    while(!acc->stop) {
        struct sockaddr_storage client_addr;
        socklen_t sin_size = sizeof(client_addr);
        int new_fd;

        // "select()" configuration
        FD_ZERO(&rfds);
        FD_SET(acc->socket_fd, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        ret_val = select(acc->socket_fd + 1, &rfds, NULL, NULL, &tv);
        if (ret_val == -1) {
            if (errno == EINTR) { // Just interrupted
                continue;
            }

            char buffer[128];
            const char *err_str = strerror_r(errno, buffer, 128);
            IPX_CTX_ERROR(acc->ctx, "(Server output) select() - failed (%s)", err_str);
            break;
        }

        if (!FD_ISSET(acc->socket_fd, &rfds)) {
            // Timeout
            continue;
        }

        new_fd = accept(acc->socket_fd, (struct sockaddr *) &client_addr, &sin_size);
        if (new_fd == -1) {
            char buffer[128];
            const char *err_str = strerror_r(errno, buffer, 128);
            IPX_CTX_ERROR(acc->ctx, "(Server output) accept() - failed (%s)", err_str);
            continue;
        }

        IPX_CTX_INFO(acc->ctx, "(Server output) Client connected: %s",
            get_client_desc(client_addr).c_str());

        // Further receptions from the socket will be disallowed
        shutdown(new_fd, SHUT_RD);
        // Add new client to the array of new clients
        pthread_mutex_lock(&acc->mutex);
        client_t new_client {client_addr, new_fd, ""};
        acc->new_clients.push_back(new_client);
        acc->new_clients_ready = true;
        pthread_mutex_unlock(&acc->mutex);
    }

    IPX_CTX_INFO(acc->ctx, "(Server output) Connection acceptor terminated.", '\0');
    return NULL;
}

/**
 * \brief Send a message to a client
 *
 * Sends the message to the client using prepared socket. When non-blocking mode
 * is enabled and only part of the message was sent, the rest of the message is
 * stored in the client's profile.
 * \param[in] data   The message
 * \param[in] len    The length of the message
 * \param[in] client Client
 * \return Transmission status
 */
enum Server::Send_status
Server::msg_send(const char *data, ssize_t len, client_t &client)
{
    ssize_t now;
    ssize_t todo = len;
    const char *ptr = data;

    int flags = MSG_NOSIGNAL;
    if (_non_blocking) {
        flags |= MSG_DONTWAIT;
    }

    while (todo > 0) {
        now = send(client.socket, ptr, todo, flags);

        if (now == -1) {
            if (_non_blocking && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // Non-blocking mode
                break;
            }

            // Connection failed
            char buffer[128];
            const char *err_str = strerror_r(errno, buffer, 128);
            IPX_CTX_INFO(_ctx, "(Server output) Client disconnected: %s (%s)",
                get_client_desc(client.info).c_str(), err_str);
            return SEND_FAILED;
        }

        ptr  += now;
        todo -= now;
    }

    if (todo <= 0) {
        return SEND_OK;
    }

    // Non-blocking mode - (partly) failed to sent the message
    if (todo == len) {
        // No part of the message was sent
        return SEND_WOULDBLOCK;
    }

    /*
     * Partly sent. Store the rest of the message for the next transmission to
     * avoid invalid JSON format. Temporary string should be here, because it is
     * possible to "data == rest.c_str()".
     */
    std::string tmp(ptr, todo);
    client.msg_rest.assign(tmp);
    return SEND_WOULDBLOCK;
}

/**
 * \brief Send record to all connected clients
 *
 * \param[in] str JSON Record
 * \param[in] len Length of the record
 * \return Always #IPX_OK
 */
int Server::process(const char *str, size_t len)
{
    const char *data = str;
    ssize_t length = len;

    // Are there new clients?
    if (_acceptor->new_clients_ready) {
        pthread_mutex_lock(&_acceptor->mutex);

        _clients.insert(_clients.end(), _acceptor->new_clients.begin(),
            _acceptor->new_clients.end());
        _acceptor->new_clients.clear();

        _acceptor->new_clients_ready = false;
        pthread_mutex_unlock(&_acceptor->mutex);
    }

    // Send the message to all clients
    std::vector<client_t>::iterator iter = _clients.begin();
    while (iter != _clients.end()) {
        client_t &client = *iter;
        enum Send_status ret_val;

        // Send the rest part of the last partly sent message
        if (_non_blocking && !client.msg_rest.empty()) {
            std::string &rest = client.msg_rest;

            ret_val = msg_send(rest.c_str(), rest.size(), client);
            switch (ret_val) {
            case SEND_OK:
                // The rest of the message successfully sent
                rest.clear();
                break;
            case SEND_WOULDBLOCK:
                // Skip, next client...
                ++iter;
                continue;
            case SEND_FAILED:
                // Close socket and remove client
                close(client.socket);
                iter = _clients.erase(iter); // The iterator has new location...
                continue;
            }
        }

        // Send new message
        ret_val = msg_send(data, length, client);
        switch (ret_val) {
        case SEND_OK:
        case SEND_WOULDBLOCK:
            // Next client
            ++iter;
            break;
        case SEND_FAILED:
            // Close socket and remove client's info
            close(client.socket);
            iter = _clients.erase(iter); // The iterator has new location...
            break;
        }
    }

    return IPX_OK;
}

/**
 * \brief Get a brief description about connected client
 * \param[in] client Client network info
 * \return String with client's IP and port
 */
std::string Server::get_client_desc(const struct sockaddr_storage &client)
{
    char ip_str[INET6_ADDRSTRLEN] = {0};
    uint16_t port;

    switch (client.ss_family) {
    case AF_INET: {
        // IPv4
        const struct sockaddr_in *src_ip = (struct sockaddr_in *) &client;
        inet_ntop(client.ss_family, &src_ip->sin_addr, ip_str, sizeof(ip_str));
        port = ntohs(src_ip->sin_port);

        return std::string(ip_str) + ":" + std::to_string(port);
        }
    case AF_INET6: {
        // IPv6
        const struct sockaddr_in6 *src_ip = (struct sockaddr_in6 *) &client;
        inet_ntop(client.ss_family, &src_ip->sin6_addr, ip_str, sizeof(ip_str));
        port = ntohs(src_ip->sin6_port);

        return std::string(ip_str) + ":" + std::to_string(port);
        }
    default:
        return "Unknown";
    }
}
