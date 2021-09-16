/**
 * \file src/plugins/output/forwarder/src/Connector.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Connector class header
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
#include <unordered_map>
#include <atomic>
#include <vector>
#include <memory>
#include <cstring>
#include <mutex>
#include <algorithm>
#include <thread>
#include <ctime>
#include <cassert>
#include <cerrno>

#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>

#include <libfds.h>
#include <ipfixcol2.h>

#include "common.h"

/**
 * \brief  Class representing a socket that will be connected in the future
 */
class FutureSocket {
public:
    /**
     * \brief  Check if the socket is ready to be retrieved
     */
    bool ready() const { return m_ready; }

    /**
     * \brief  Retrieve the result, i.e. the socket
     */
    UniqueSockfd retrieve() { m_ready = false; return std::move(m_sockfd); }

    /**
     * \brief  Set the result and make it ready for retrieval
     */
    void set_result(UniqueSockfd sockfd) { m_sockfd = std::move(sockfd); m_ready = true; }

private:
    UniqueSockfd m_sockfd;
    std::atomic<bool> m_ready{false};
};

/**
 * \brief  Simple utility class around pipe used for interrupting poll
 */
class Pipe {
public:
    /**
     * \brief  The constructor
     */
    Pipe();

    Pipe(const Pipe &) = delete;
    Pipe(Pipe &&) = delete;

    /**
     * \brief  Write to the pipe to trigger its write event
     * \param ignore_error  Do not throw on write error
     * \throw std::runtime_erorr on write error if ignore_error is false
     */
    void poke(bool ignore_error = false);

    /**
     * \brief  Read everything from the pipe and throw it away
     */
    void clear();

    /**
     * \brief  Get the read file descriptor
     */
    int readfd() const { return m_readfd; }

    /**
     * \brief  The destructor
     */
    ~Pipe();

private:
    int m_readfd = -1;
    int m_writefd = -1;
};

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
    struct Task {
        enum class State { Created, AddrResolved, Connecting, Connected, Completed, Errored };

        Task(ConnectionParams params, time_t start_time = 0) : params(params), start_time(start_time) {}

        ConnectionParams params;
        time_t start_time;
        State state = State::Created;
        UniqueSockfd sockfd;
        std::unique_ptr<struct addrinfo, decltype(&freeaddrinfo)> addrs{nullptr, &freeaddrinfo};
        struct addrinfo *next_addr = nullptr;
    };

    // Reconnect interval
    unsigned int m_reconnect_secs;
    // Mutex for shared state
    std::mutex m_mutex;
    // The possible hosts
    std::vector<ConnectionParams> m_hosts;
    // Tasks to be retrieved by the worker thread
    std::vector<Task> m_incoming_tasks;
    // The tasks
    std::vector<Task> m_tasks;
    // Socket requests waiting to be fulfilled
    std::unordered_map<ConnectionParams, std::vector<std::shared_ptr<FutureSocket>>, ConnectionParams::Hasher> m_requests;
    // Extra sockets
    std::unordered_map<ConnectionParams, std::vector<UniqueSockfd>, ConnectionParams::Hasher> m_extra;
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

    void
    resubmit_task(ConnectionParams host);

    void
    task_resolve_addr(Task &task);

    void
    task_check_connected(Task &task);

    void
    task_connect(Task &task);

    void
    task_complete(Task &task);

    void
    process_task(Task &task);

    void
    main_loop();
};
