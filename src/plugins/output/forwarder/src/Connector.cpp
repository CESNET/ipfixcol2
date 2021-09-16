/**
 * \file src/plugins/output/forwarder/src/Connector.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Connector class implementation
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

#include "Connector.h"

#include "ipfixcol2.h"

Pipe::Pipe()
{
    int fd[2];
    if (pipe(fd) != 0) {
        throw errno_runtime_error(errno, "pipe");
    }

    m_readfd = fd[0];
    m_writefd = fd[1];

    int flags;

    if ((flags = fcntl(m_readfd, F_GETFL)) == -1) {
        throw errno_runtime_error(errno, "fcntl");
    }
    if (fcntl(m_readfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw errno_runtime_error(errno, "fcntl");
    }

    if ((flags = fcntl(m_writefd, F_GETFL)) == -1) {
        throw errno_runtime_error(errno, "fcntl");
    }
    if (fcntl(m_writefd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw errno_runtime_error(errno, "fcntl");
    }

}


void
Pipe::poke(bool ignore_error)
{
    ssize_t ret = write(m_writefd, "\x00", 1);
    if (ret < 0 && !ignore_error) {
        throw errno_runtime_error(errno, "write");
    }
}

void
Pipe::clear()
{
    char throwaway[16];
    while (read(m_readfd, throwaway, 16) > 0) {}
}

Pipe::~Pipe()
{
    if (m_readfd >= 0) {
        close(m_readfd);
    }

    if (m_writefd >= 0) {
        close(m_writefd);
    }
}


Connector::Connector(const std::vector<ConnectionParams> &hosts, unsigned int nb_premade_connections,
                     unsigned int reconnect_secs, ipx_ctx_t *log_ctx) :
    m_reconnect_secs(reconnect_secs),
    m_log_ctx(log_ctx),
    m_nb_premade_connections(nb_premade_connections)
{
    // Set up tasks for premade connections
    for (auto &host : hosts) {
        for (size_t i = 0; i < nb_premade_connections; i++) {
            m_incoming_tasks.push_back(Task(host));
        }
    }

    m_thread = std::thread([this](){ this->main_loop(); });
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
    // Shared state is accessed
    std::unique_lock<std::mutex> lock(m_mutex);

    // Add a new task and notify the worker thread
    m_incoming_tasks.push_back(Task(host));
    m_statpipe.poke();

    // Create shared future
    std::shared_ptr<FutureSocket> future(std::make_shared<FutureSocket>());

    auto &extra = m_extra[host];
    // If there are premade connections return one of those instead,
    // the created future will become a new premade connection to replace the one we take
    if (!extra.empty()) {
        future->set_result(std::move(extra.front()));
        extra.erase(extra.begin());
        return future;
    }

    // No premade connections - return the future we just created
    m_requests[host].push_back(future);
    return future;
}

void
Connector::resubmit_task(ConnectionParams host)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    int required = 0;

    // Count futures for this host that are still waiting for a socket
    for (const auto &future : m_requests[host]) {
        // If the use_count is 1 it's only the Connector holding a reference to it -> the future was cancelled
        if (future.use_count() > 1) {
            required++;
        }
    }

    // How many more premade connections do we need
    required += m_nb_premade_connections - m_extra[host].size();

    // How many tasks that might still successfully complete are there
    for (const auto &task : m_tasks) {
        if (task.params == host && task.state != Task::State::Completed && task.state != Task::State::Errored) {
            required--;
        }
    }

    // Don't restart the task, there is no need
    if (required <= 0) {
        return;
    }

    // Reschedule the failed task to be ran after reconnect interval elapses
    time_t now = get_monotonic_time();
    m_incoming_tasks.push_back(Task(host, now + m_reconnect_secs));
}

void
Connector::task_resolve_addr(Task &task)
{
    struct addrinfo *ai;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;

    switch (task.params.protocol) {
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

    int ret = getaddrinfo(task.params.address.c_str(),
                        std::to_string(task.params.port).c_str(),
                        &hints, &ai);
    if (ret != 0) {
        IPX_CTX_WARNING(m_log_ctx, "getaddrinfo() failed: %s", gai_strerror(ret));
        task.state = Task::State::Errored;
        return;
    }

    task.addrs.reset(ai);
    task.next_addr = task.addrs.get();
    task.state = Task::State::AddrResolved;
}

void
Connector::task_check_connected(Task &task)
{
    int optval;
    socklen_t optlen = sizeof(optval);

    if (getsockopt(task.sockfd.get(), SOL_SOCKET, SO_ERROR, &optval, &optlen) == -1) {
        char *errbuf;
        ipx_strerror(errno, errbuf);
        IPX_CTX_ERROR(m_log_ctx, "getsockopt() failed: %s", errbuf);
        task.state = Task::State::Errored;
        return;
    }

    if (optval != 0) { // optval is the errno of the non-blocking operation
        char *errbuf;
        ipx_strerror(optval, errbuf);
        IPX_CTX_WARNING(m_log_ctx, "connect() failed: %s", errbuf);
        task.state = Task::State::Errored;
        return;
    }

    task.state = Task::State::Connected;
}

void
Connector::task_connect(Task &task)
{
    struct addrinfo *addr = task.next_addr;

    int sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (sockfd < 0) {
        char *errbuf;
        ipx_strerror(errno, errbuf);
        IPX_CTX_ERROR(m_log_ctx, "socket() failed: %s", errbuf);
        task.state = Task::State::Errored;
        return;
    }
    task.sockfd.reset(sockfd);

    int flags;
    if ((flags = fcntl(task.sockfd.get(), F_GETFL)) == -1) {
        char *errbuf;
        ipx_strerror(errno, errbuf);
        IPX_CTX_ERROR(m_log_ctx, "fcntl() failed: %s", errbuf);
        task.state = Task::State::Errored;
        return;
    }
    if (fcntl(task.sockfd.get(), F_SETFL, flags | O_NONBLOCK) == -1) {
        char *errbuf;
        ipx_strerror(errno, errbuf);
        IPX_CTX_ERROR(m_log_ctx, "fcntl() failed: %s", errbuf);
        task.state = Task::State::Errored;
        return;
    }

    if (connect(task.sockfd.get(), addr->ai_addr, addr->ai_addrlen) != 0 && errno != EINPROGRESS) {
        char *errbuf;
        ipx_strerror(errno, errbuf);
        IPX_CTX_WARNING(m_log_ctx, "connect() failed: %s", errbuf);
        task.state = Task::State::Errored;
        return;
    }

    task.state = Task::State::Connecting;
}

void
Connector::task_complete(Task &task)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    auto &reqs = m_requests[task.params];
    auto &extra = m_extra[task.params];

    if (reqs.empty()) {
        if (extra.size() < m_nb_premade_connections) {
            extra.push_back(std::move(task.sockfd));
        }

    } else {
        auto req = reqs.front();
        reqs.erase(reqs.begin());
        req->set_result(std::move(task.sockfd));
    }

    task.state = Task::State::Completed;
}

void
Connector::process_task(Task &task)
{
    if (task.state == Task::State::Created) {
        task_resolve_addr(task);
        if (task.state == Task::State::Errored) {
            resubmit_task(task.params);
            return;
        }

        task_connect(task);

    } else if (task.state == Task::State::Connecting) {
        task_check_connected(task);

        if (task.state == Task::State::Connected) {
            task_complete(task);
            return;
        }

        task.next_addr = task.next_addr->ai_next;
        if (!task.next_addr) {
            task.state = Task::State::Errored;
            resubmit_task(task.params);
            return;
        }

        task_connect(task);

    } else {
        assert(0);
    }
}

void
Connector::main_loop()
{
    std::vector<struct pollfd> pollfds;
    std::vector<Task *> poll_tasks;

    while (!m_stop_flag) {
        IPX_CTX_DEBUG(m_log_ctx, "Connector task count: %zu", m_tasks.size());

        // Process polled tasks
        for (size_t i = 0; i < poll_tasks.size(); i++) {
            if (pollfds[i].revents) {
                process_task(*poll_tasks[i]);
            }
        }
        pollfds.clear();
        poll_tasks.clear();

        { // Critical section
            std::unique_lock<std::mutex> lock(m_mutex);

            // Cleanup futures
            for (auto p : m_requests) {
                auto &reqs = p.second;
                reqs.erase(
                    std::remove_if(reqs.begin(), reqs.end(),
                        [](const std::shared_ptr<FutureSocket> &future) { return future.use_count() == 1; }
                    ), reqs.end());
            }

            // Get new tasks
            m_tasks.insert(
                m_tasks.end(),
                std::make_move_iterator(m_incoming_tasks.begin()),
                std::make_move_iterator(m_incoming_tasks.end()));
            m_incoming_tasks.clear();
        }

        // Start tasks that are not yet started
        time_t now = get_monotonic_time();

        for (auto &task : m_tasks) {
            if (task.state == Task::State::Created && task.start_time <= now) {
                process_task(task);
            }

            if (task.state == Task::State::Connecting) {
                struct pollfd pfd;
                pfd.fd = task.sockfd.get();
                pfd.events = POLLOUT; // When connect succeeds the socket is ready to write -> POLLOUT event

                pollfds.push_back(pfd);
                poll_tasks.push_back(&task);
            }
        }

        // Cleanup finished or errored tasks
        m_tasks.erase(
            std::remove_if(m_tasks.begin(), m_tasks.end(),
                [](const Task &task) { return task.state == Task::State::Completed || task.state == Task::State::Errored; }
            ),
            m_tasks.end());

        // Also poll pipe to communicate state changes
        struct pollfd pfd;
        pfd.fd = m_statpipe.readfd();
        pfd.events = POLLIN;
        pollfds.push_back(pfd);

        // Wait for one of the sockets or the status fd event
        if (poll(pollfds.data(), pollfds.size(), m_reconnect_secs * 1000) < 0) {
            char *errbuf;
            ipx_strerror(errno, errbuf);
            IPX_CTX_ERROR(m_log_ctx, "poll() failed: %s", errbuf);
        }

        // Empty the pipe as it's only used to stop the poll
        m_statpipe.clear();
    }
}
