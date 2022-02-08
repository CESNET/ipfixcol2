/**
 * \file src/plugins/output/forwarder/src/connector/Connector.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Connector class
 * \date 2021
 */

/* Copyright (C) 2021 CESNET, z.s.p.o.
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

#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <thread>
#include <atomic>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <ipfixcol2.h>

#include "common.h"
#include "connector/Pipe.h"
#include "connector/FutureSocket.h"

/**
 * \brief  A connector class that handles socket connections on a separate thread
 */
class Connector {
public:

    /**
     * \brief  The constructor
     *
     * \param hosts                   The list of hosts connections will be made to
     * \param nb_premade_connections  Number of extra open connections to keep
     * \param reconnect_secs          The reconnect interval
     * \param log_ctx                 The logging context
     */
    Connector(const std::vector<ConnectionParams> &hosts, unsigned int nb_premade_connections,
              unsigned int reconnect_secs, ipx_ctx_t *log_ctx);

    // No copying or moving
    Connector(const Connector&) = delete;
    Connector(Connector&&) = delete;

    /**
     * \brief  Get a connected socket to the host
     *
     * \param host  The host
     * \return A socket that is connected or will be connected in the future
     */
    std::shared_ptr<FutureSocket>
    get(const ConnectionParams &host);

    /**
     * \brief  The destructor
     */
    ~Connector();

private:
    struct Request {
        // The connection parameters
        ConnectionParams params;
        // The future also owned by the caller
        std::shared_ptr<FutureSocket> future;
    };

    struct Task {
        enum class State { NotStarted, Connecting, Connected, ToBeDeleted };
        // The connection parameters
        ConnectionParams params;
        // State of the task
        State state = State::NotStarted;
        // When should the task start
        time_t start_time = 0;
        // The socket
        UniqueFd sockfd;
        // The resolved addresses
        std::unique_ptr<struct addrinfo, decltype(&freeaddrinfo)> addrs{nullptr, &freeaddrinfo};
        // The next address to try
        struct addrinfo *next_addr = nullptr;
    };

    // Reconnect interval
    unsigned int m_reconnect_secs;
    // Mutex for shared state
    std::mutex m_mutex;
    // Requests to be retrieved by the worker thread
    std::vector<Request> m_new_requests;
    // The requests - exclusively handled by the worker thread!
    std::vector<Request> m_requests;
    // The tasks - exclusively handled by the worker thread!
    std::vector<Task> m_tasks;
    // Pipe to notify for status changes
    Pipe m_statpipe;
    // The worker thread
    std::thread m_thread;
    // Stop flag for the worker thread
    std::atomic<bool> m_stop_flag{false};
    // The logging context
    ipx_ctx_t *m_log_ctx;
    // Number of premade connections to keep
    unsigned int m_nb_premade_connections;
    // The poll fds
    std::vector<struct pollfd> m_pollfds;

    void
    wait_for_poll_event();

    void
    cleanup_tasks();

    void
    process_requests();

    void
    setup_pollfds();

    void
    start_tasks();

    void
    process_poll_events();

    void
    main_loop();

    void
    run();

    void
    on_task_start(Task &task);

    void
    on_task_poll_event(Task &task, int events);

    void
    on_task_connected(Task &task);

    void
    on_task_failed(Task &task);

    bool
    should_restart(Task &task);
};
