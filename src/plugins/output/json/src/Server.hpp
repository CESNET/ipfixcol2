/**
 * \file src/plugins/output/json/src/Server.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Server output (header file)
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

#ifndef JSON_SERVER_H
#define JSON_SERVER_H

#include <atomic>
#include <string>
#include <vector>
#include <pthread.h>
#include <sys/socket.h>

#include "Storage.hpp"

/**
 * \brief The class for server output interface
 */
class Server : public Output
{
public:
    Server(const struct cfg_server &cfg, ipx_ctx_t *ctx);
    ~Server();

    // Send a record to connected clients
    int process(const char *str, size_t len);
private:
    /** Transmission status */
    enum Send_status {
        SEND_OK,               /**< Successfully sent                             */
        SEND_WOULDBLOCK,       /**< Message skipped or partly sent                */
        SEND_FAILED            /**< Failed                                        */
    };

    /** Structure for connected client */
    typedef struct client_s {
        struct sockaddr_storage info; /**< Info about client (IP, port)           */
        int socket;                   /**< Client's socket                        */
        std::string msg_rest;         /**< Rest of a message in non-blocking mode */
    } client_t;

    /** Configuration of acceptor thread */
    typedef struct acceptor_s {
        ipx_ctx_t *ctx;                     /**< Instance context (for log only ) */
        pthread_t thread;					/**< Thread                           */
        pthread_mutex_t mutex;              /**< Mutex for the array              */
        std::atomic<bool> stop;             /**< Stop flag for terminating        */

        int socket_fd;                      /**< Server socket                    */
        std::atomic<bool> new_clients_ready; /**< New clients flag                 */
        std::vector<client_t> new_clients;  /**< Array of new clients             */
    } acceptor_t;

    /** Connected client */
    std::vector<client_t> _clients;
    /** Socket state */
    bool _non_blocking;
    /** Acceptor of new clients */
    acceptor_t *_acceptor;

    // Brief description of a client
    static std::string get_client_desc(const struct sockaddr_storage &client);
    // Send data to the client
    enum Send_status msg_send(const char *data, ssize_t len, client_t &client);

    // Acceptor's thread function
    static void *thread_accept(void *context);
};

#endif // JSON_SERVER_H
