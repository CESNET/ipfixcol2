/**
 * \file src/plugins/output/json/src/Sender.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Sender output (source file)
 * \date 2018
 */

/* Copyright (C) 2018 CESNET, z.s.p.o.
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
 * This software is provided ``as is'', and any express or implied
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

#include "Sender.hpp"

#include <time.h>
#include <unistd.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

/** Value of invalid file (socket) descriptor      */
#define INVALID_FD (-1)
/** Delay between reconnection attempts (seconds)  */
#define RECONN_DELAY (5)

/**
 * \brief Class constructor
 * \param[in] cfg Sender configuration
 * \param[in] ctx Instance context
 */
Sender::Sender(const struct cfg_send &cfg, ipx_ctx_t *ctx) : Output(cfg.name, ctx)
{
    params = cfg;
    sd = INVALID_FD;
    clock_gettime(CLOCK_MONOTONIC, &connection_time);

    // Create a new connection
    connect();
}

/** Destructor */
Sender::~Sender()
{
    if (sd != INVALID_FD) {
        close(sd);
    }
}

/**
 * \brief Send a JSON record
 * \param[in] str JSON Record to send
 * \param[in] len Size of the record
 * \return Always #IPX_OK
 */
int
Sender::process(const char *str, size_t len)
{
    if (sd == INVALID_FD) {
        // Not connected -> try to reconnect
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        // Try only one reconnection per second
        if (connection_time.tv_sec + RECONN_DELAY > now.tv_sec) {
            return IPX_OK;
        }

        connection_time = now;
        if (connect() != IPX_OK) {
            IPX_CTX_WARNING(_ctx, "(Send output) Reconnection to '%s:%" PRIu16 "' failed! "
                "Trying again in %d seconds.", params.addr.c_str(), params.port, int(RECONN_DELAY));
            return IPX_OK;
        } else {
            IPX_CTX_INFO(_ctx, "(Send output) Successfully connected to '%s:%" PRIu16 "'.",
                params.addr.c_str(), params.port);
        }
    }

    // Send not previously send data (only for non-blocking mode)
    enum Send_status status;
    if (!params.blocking && !msg_rest.empty()) {
        status = send(msg_rest.c_str(), msg_rest.size());
        switch (status) {
        case SEND_OK:
            msg_rest.clear();
            break;
        case SEND_WOULDBLOCK:
            // Nothing to send
            return IPX_OK;
        case SEND_FAILED:
            close(sd);
            sd = INVALID_FD;
            return IPX_OK;
        }
    }

    // Send new data
    status = send(str, len);
    switch (status) {
    case SEND_OK:
    case SEND_WOULDBLOCK:
        // Nothing to do
        break;
    case SEND_FAILED:
        close(sd);
        sd = INVALID_FD;
        break;
    }

    return IPX_OK;
}

/**
 * \brief Create a new connection to the destination
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED in case of connection failure
 */
int
Sender::connect()
{
    if (sd != INVALID_FD) {
        close(sd);
        sd = INVALID_FD;
    }

    // Create a new socket
    std::string port_str = std::to_string(params.port);
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = (params.proto == cfg_send::SEND_PROTO_TCP) ? SOCK_STREAM : SOCK_DGRAM;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_NUMERICSERV;

    struct addrinfo *result, *ptr;
    int rc;
    if ((rc = getaddrinfo(params.addr.c_str(), port_str.c_str(), &hints, &result)) != 0) {
        IPX_CTX_ERROR(_ctx, "(Send output) getaddrinfo() failed: %s", gai_strerror(rc));
        return IPX_ERR_DENIED;
    }

    for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        // Try to create a new socket and connect
        sd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (sd == INVALID_FD) {
            continue;
        }

        if (::connect(sd, ptr->ai_addr, ptr->ai_addrlen) == -1) {
            close(sd);
            continue;
        }

        // Success
        break;
    }

    freeaddrinfo(result);
    if (ptr == nullptr) {
        IPX_CTX_ERROR(_ctx, "(Send output) Unable to connect to '%s:%" PRIu16 "'!",
            params.addr.c_str(), params.port);
        sd = INVALID_FD;
        return IPX_ERR_DENIED;
    }

    return IPX_OK;
}

/**
 * \brief Send a JSON record
 *
 * If non-blocking mode is enabled and only part of the record is sent, the rest is stored
 * into a buffer.
 * \param[in] str The record to send
 * \param[in] len Length of the record
 * \return #SEND_OK on success
 * \return #SEND_WOULDBLOCK if a part or nothing of the message was sent
 * \return #SEND_FAILED in case of broken connection
 */
enum Sender::Send_status
Sender::send(const char *str, size_t len)
{
    ssize_t now;
    size_t todo = len;
    const char *ptr = str;

    int flags = MSG_NOSIGNAL;
    if (!params.blocking) {
        flags |= MSG_DONTWAIT;
    }

    while (todo > 0) {
        now = ::send(sd, ptr, todo, flags);
        if (now == -1) {
            if (!params.blocking && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // Non-blocking mode
                break;
            }

            // Connection failed
            char buffer[128];
            const char *err_str = strerror_r(errno, buffer, 128);
            IPX_CTX_INFO(_ctx, "(Send output) Destination '%s:%" PRIu16 "' disconnected: %s",
                params.addr.c_str(), params.port, err_str);
            return SEND_FAILED;
        }

        ptr  += now;
        todo -= now;
    }

    if (todo == 0) {
        return SEND_OK;
    }

    // Non-blocking mode - (partly) failed to sent the message
    if (todo == len) {
        // No part of the message was sent
        return SEND_WOULDBLOCK;
    }

    /*
     * Partly sent. Store the rest of the message for the next transmission to
     * avoid invalid JSON format.
     */
    std::string tmp(ptr, todo);
    msg_rest.assign(tmp);
    return SEND_WOULDBLOCK;
}