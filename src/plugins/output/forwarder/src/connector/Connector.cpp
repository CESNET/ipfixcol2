/**
 * \file src/plugins/output/forwarder/src/connector/Connector.cpp
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

#include "connector/Connector.h"

#include <algorithm>
#include <cinttypes>

#include <fcntl.h>
#include <unistd.h>
#include <poll.h>

Connector::Connector(const std::vector<ConnectionParams> &hosts, unsigned int nb_premade_connections,
                     unsigned int reconnect_secs, ipx_ctx_t *log_ctx) :
    m_reconnect_secs(reconnect_secs),
    m_log_ctx(log_ctx),
    m_nb_premade_connections(nb_premade_connections)
{
    // Set up tasks for premade connections
    for (auto &host : hosts) {
        for (size_t i = 0; i < nb_premade_connections; i++) {
            Task task = {};
            task.params = host;
            // The worker thread isn't running yet, so we can access m_tasks from here.
            m_tasks.emplace_back(std::move(task));
        }
    }

    // Start the worker thread
    m_thread = std::thread([this](){ this->run(); });
}

Connector::~Connector()
{
    // Let the worker thread know that we're stopping
    m_stop_flag = true;
    m_statpipe.poke(false);
    m_thread.join();
}

std::shared_ptr<FutureSocket>
Connector::get(const ConnectionParams &host)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    std::shared_ptr<FutureSocket> future(std::make_shared<FutureSocket>());
    m_new_requests.emplace_back(Request{host, future});
    m_statpipe.poke();

    return future;
}

/**
 * Advance execution of tasks where a poll event occured
 */
void
Connector::process_poll_events()
{
    if (m_pollfds.size() == 0) {
        return;
    }

    for (size_t i = 0; i < m_pollfds.size() - 1; i++) { // - 1 because the last one is the pipe
        if (!m_pollfds[i].revents) {
            continue;
        }
        /*
        const char *statestr[] = {"NotStarted", "Connecting", "Connected", "ToBeDeleted"};
        IPX_CTX_INFO(m_log_ctx, "[%d] Poll event %u Task %s:%" PRIu16 " State %s", i, m_pollfds[i].revents,
                     m_tasks[i].params.address.c_str(), m_tasks[i].params.port, statestr[(int)m_tasks[i].state]);
        */
        try {
            on_task_poll_event(m_tasks[i], m_pollfds[i].revents);

        } catch (const std::runtime_error &err) {
            IPX_CTX_INFO(m_log_ctx, "Connecting to %s:%" PRIu16 " failed - %s",
                         m_tasks[i].params.address.c_str(), m_tasks[i].params.port, err.what());
            on_task_failed(m_tasks[i]);
        }
    }
}

/**
 * Start tasks that hasn't been started yet and should be
 */
void
Connector::start_tasks()
{
    time_t now = get_monotonic_time();

    for (auto &task : m_tasks) {
        if (task.state != Task::State::NotStarted || task.start_time > now) {
            continue;
        }

        try {
            on_task_start(task);

        } catch (const std::runtime_error &err) {
            IPX_CTX_INFO(m_log_ctx, "Connecting to %s:%" PRIu16 " failed - %s",
                         task.params.address.c_str(), task.params.port, err.what());
            on_task_failed(task);
        }

    }
}

/**
 * Populate the vector of pollfds to listen for events on sockets of tasks that are connecting or connected
 */
void
Connector::setup_pollfds()
{
    m_pollfds.resize(m_tasks.size() + 1);

    size_t i = 0;
    for (const auto &task : m_tasks) {
        struct pollfd &poll_fd = m_pollfds[i++];
        poll_fd.fd = 0;
        poll_fd.events = 0;
        poll_fd.revents = 0;

        if (task.state == Task::State::Connecting) {
            poll_fd.fd = task.sockfd.get();
            poll_fd.events = POLLOUT;

        } else if (task.state == Task::State::Connected) {
            poll_fd.fd = task.sockfd.get();
        }
    }

    struct pollfd &poll_fd = m_pollfds[i++];
    poll_fd.fd = m_statpipe.readfd();
    poll_fd.events = POLLIN;
}

/**
 * Process new requests from the caller and delete dead requests
 */
void
Connector::process_requests()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // Delete dead requests, e.g. if the caller requested a connection for a new session, but the session closed before
    // the connection was estabilished
    m_requests.erase(std::remove_if(m_requests.begin(), m_requests.end(),
                                    [](const Request &request) { return request.future.use_count() == 1; }
                                    ), m_requests.end());

    // Collect new requests
    for (auto &new_request : m_new_requests) {
        bool found = false;

        // Check if there is premade connection that can be used to fulfil the request
        for (auto &task : m_tasks) {
            if (task.state == Task::State::Connected && task.params == new_request.params) {
                new_request.future->set(std::move(task.sockfd));

                task.start_time = 0;
                task.state = Task::State::NotStarted;

                found = true;
                break;
            }
        }

        // Create task for the request
        if (!found) {
            Task task = {};
            task.params = new_request.params;
            m_tasks.emplace_back(std::move(task));

            m_requests.emplace_back(std::move(new_request));
        }
    }

    m_new_requests.clear();
}

/**
 * Delete tasks that are marked for deletion
 */
void
Connector::cleanup_tasks()
{
    m_tasks.erase(std::remove_if(m_tasks.begin(), m_tasks.end(),
                                 [](const Task &task) { return task.state == Task::State::ToBeDeleted; }),
                  m_tasks.end());
}

/**
 * Wait for a poll event on any of the task sockets or the status pipe
 */
void
Connector::wait_for_poll_event()
{
    // Wait for one of the sockets or the status fd event
    if (poll(m_pollfds.data(), m_pollfds.size(), 1000) < 0) {
        char *errbuf;
        ipx_strerror(errno, errbuf);
        IPX_CTX_ERROR(m_log_ctx, "poll() failed: %s", errbuf);
    }

    // Empty the pipe as it's only used to stop the poll
    m_statpipe.clear();
}

/**
 * The main loop
 */
void
Connector::main_loop()
{
    while (!m_stop_flag) {
        process_poll_events();

        process_requests();

        start_tasks();

        cleanup_tasks();

        /*
        IPX_CTX_INFO(m_log_ctx, "Tasks: %d, Requests: %d", m_tasks.size(), m_requests.size());
        int i = 0;
        for (const auto &task : m_tasks) {
            i++;
            const char *statestr[] = {"NotStarted", "Connecting", "Connected", "ToBeDeleted"};
            IPX_CTX_INFO(m_log_ctx, "[%d] Task %s:%" PRIu16 "  State %s  StartTime %d", i,
                         task.params.address.c_str(), task.params.port, statestr[(int)task.state], task.start_time);
        }
        */
        setup_pollfds();

        wait_for_poll_event();
    }
}

/**
 * The entry point of the worker thread
 */
void
Connector::run()
{
    try {
        main_loop();

    } catch (const std::bad_alloc &ex) {
        IPX_CTX_ERROR(m_log_ctx, "Caught exception in connector thread: Memory error", 0);
        IPX_CTX_ERROR(m_log_ctx, "Fatal error, connector stopped!", 0);

    } catch (const std::runtime_error &ex) {
        IPX_CTX_ERROR(m_log_ctx, "Caught exception in connector thread: %s", ex.what());
        IPX_CTX_ERROR(m_log_ctx, "Fatal error, connector stopped!", 0);

    } catch (const std::exception &ex) {
        IPX_CTX_ERROR(m_log_ctx, "Caught exception in connector thread: %s", ex.what());
        IPX_CTX_ERROR(m_log_ctx, "Fatal error, connector stopped!", 0);

    } catch (...) {
        IPX_CTX_ERROR(m_log_ctx, "Caught exception in connector thread", 0);
        IPX_CTX_ERROR(m_log_ctx, "Fatal error, connector stopped!", 0);
    }
}

/**********************************************************************************************************************/

/**
 * Wrapper around getaddrinfo
 */
static std::unique_ptr<struct addrinfo, decltype(&freeaddrinfo)>
resolve_addrs(const ConnectionParams &params)
{
    struct addrinfo *ai;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;

    switch (params.protocol) {
    case Protocol::TCP:
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_socktype = SOCK_STREAM;
        break;

    case Protocol::UDP:
        hints.ai_protocol = IPPROTO_UDP;
        hints.ai_socktype = SOCK_DGRAM;
        break;

    default: assert(0);
    }

    int ret = getaddrinfo(params.address.c_str(), std::to_string(params.port).c_str(), &hints, &ai);
    if (ret != 0) {
        throw std::runtime_error(std::string("getaddrinfo() failed: ") + gai_strerror(ret));
    }

    return {ai, &freeaddrinfo};
}

/**
 * Create a socket and begin non-blocking connect to the provided address
 */
static UniqueFd
create_and_connect_socket(struct addrinfo *addr)
{
    UniqueFd sockfd;
    int raw_sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (raw_sockfd < 0) {
        throw errno_runtime_error(errno, "socket");
    }
    sockfd.reset(raw_sockfd);

    int flags;
    if ((flags = fcntl(sockfd.get(), F_GETFL)) == -1) {
        throw errno_runtime_error(errno, "fcntl");
    }
    if (fcntl(sockfd.get(), F_SETFL, flags | O_NONBLOCK) == -1) {
        throw errno_runtime_error(errno, "fcntl");
    }

    if (addr->ai_socktype == SOCK_STREAM) {
        int optval = 1;
        if (setsockopt(sockfd.get(), SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) != 0) {
            throw errno_runtime_error(errno, "setsockopt");
        }
    }

    if (connect(sockfd.get(), addr->ai_addr, addr->ai_addrlen) != 0 && errno != EINPROGRESS) {
        throw errno_runtime_error(errno, "connect");
    }

    return sockfd;
}

/**
 * Check if socket error from a non-blocking operation occured and throw it as a std::runtime_error
 */
static void
throw_if_socket_error(int sockfd)
{
    int optval;
    socklen_t optlen = sizeof(optval);

    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) == -1) {
        throw errno_runtime_error(errno, "getsockopt");
    }

    if (optval != 0) { // optval is the errno of the non-blocking operation
        throw errno_runtime_error(errno, "connect");
    }
}

/**
 * Connect to the next address and advance the next address pointer
 */
static UniqueFd
connect_next(struct addrinfo *&next_addr)
{
    assert(next_addr != nullptr);

    while (next_addr) {
        struct addrinfo *addr = next_addr;
        next_addr = next_addr->ai_next;
        try {
            // This might fail right away even though it is non-blocking connect
            return create_and_connect_socket(addr);

        } catch (const std::runtime_error &err) {
            // Ignore unless this is the last address
            if (!next_addr) {
                throw err;
            }
        }
    }

    return {};
}

/**
 * Handle task start
 */
void
Connector::on_task_start(Task &task)
{
    assert(task.state == Task::State::NotStarted);

    task.addrs = resolve_addrs(task.params);
    task.next_addr = task.addrs.get();
    task.sockfd = connect_next(task.next_addr);
    task.state = Task::State::Connecting;
}

/**
 * Handle poll event on the task socket
 */
void
Connector::on_task_poll_event(Task &task, int events)
{
    // If the connection is connected and we got a poll event, the connection probably dropped
    if (task.state == Task::State::Connected) {
        throw_if_socket_error(task.sockfd.get());

        if (events & POLLERR) {
            // Just in case getsockopt(SO_ERROR) didn't return any error but we got an error from the poll event,
            // not sure if this can happen, but just to be safe
            throw std::runtime_error("socket error");
        }

        return;
    }

    assert(task.state == Task::State::Connecting);
    // Otherwise we're connecting and the poll event is either connection success or connection error
    try {
        throw_if_socket_error(task.sockfd.get());
        // No error -> connection successful
        on_task_connected(task);

    } catch (const std::runtime_error &err) {
        if (!task.next_addr) {
            throw err;
        }
        task.sockfd = connect_next(task.next_addr);
    }

}

/**
 * Handle successful connection of the task socket
 */
void
Connector::on_task_connected(Task &task)
{
    IPX_CTX_INFO(m_log_ctx, "Connecting to %s:%" PRIu16 " successful",
                 task.params.address.c_str(), task.params.port);

    std::unique_lock<std::mutex> lock(m_mutex);

    // If there is a request for this connection, complete it
    for (auto it = m_requests.begin(); it != m_requests.end(); it++) {
        Request &request = *it;
        if (request.params == task.params && request.future.use_count() > 1) {
            request.future->set(std::move(task.sockfd));
            m_requests.erase(it);

            // If this was a premade connection, restart the task
            if (!should_restart(task)) {
                task.state = Task::State::ToBeDeleted;
            } else {
                task.start_time = 0;
                task.state = Task::State::NotStarted;
            }
            return;
        }
    }

    // Count how many extra connections for this host there currently are
    unsigned int count = 0;
    for (const auto &task_ : m_tasks) {
        if (task_.state == Task::State::Connected && task.params == task_.params) {
            count++;
        }
    }

    if (count > m_nb_premade_connections) {
        // Too many - discard the connection
        task.sockfd.reset();
        task.state = Task::State::ToBeDeleted;
        return;
    }

    // Keep the connection as extra
    task.state = Task::State::Connected;
}

/**
 * Handle failure of the task
 */
void
Connector::on_task_failed(Task &task)
{
    std::unique_lock<std::mutex> lock(m_mutex);
#if 0
    // Count how many connections are required to see if we want to restart the task or throw it away.
    // This is needed because a connection request may be cancelled before it is resolved, in which case we might
    // end up with too many extra connections.
    long long required = m_nb_premade_connections;

    // Count requests for this host that are still waiting for a socket
    for (const auto &request : m_requests) {
        // If the use_count is 1 it's only the Connector holding a reference to it -> the future was cancelled
        if (task.params == request.params && request.future.use_count() > 1) {
            required++;
        }
    }

    // How many tasks that are connected or might still successfully connect are there
    for (const auto &task_ : m_tasks) {
        if (task_.state != Task::State::ToBeDeleted && task.params == task_.params) {
            required--;
        }
    }
#endif
    // Don't restart the task, there is no need
    if (!should_restart(task)) {
        task.state = Task::State::ToBeDeleted;
        return;
    }

    // Reschedule the failed task to be ran after reconnect interval elapses
    time_t now = get_monotonic_time();
    task.start_time = now + m_reconnect_secs;
    task.state = Task::State::NotStarted;

    IPX_CTX_INFO(m_log_ctx, "Retrying connection to %s:%" PRIu16 " in %u seconds",
             task.params.address.c_str(), task.params.port, m_reconnect_secs);
}

bool
Connector::should_restart(Task &task)
{
    // Count how many connections are required to see if we want to restart the task or throw it away.
    // This is needed because a connection request may be cancelled before it is resolved, in which case we might
    // end up with too many extra connections.
    long long required = m_nb_premade_connections;

    // Count requests for this host that are still waiting for a socket
    for (const auto &request : m_requests) {
        // If the use_count is 1 it's only the Connector holding a reference to it -> the future was cancelled
        if (task.params == request.params && request.future.use_count() > 1) {
            required++;
        }
    }

    // How many tasks that are connected or might still successfully connect are there
    for (const auto &task_ : m_tasks) {
        if (&task != &task_ && task_.state != Task::State::ToBeDeleted && task.params == task_.params) {
            required--;
        }
    }

    return required > 0;
}