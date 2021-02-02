/**
 * \file src/plugins/output/forwarder/src/ConnectionManager.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Connection manager
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

#include "ConnectionManager.h"

Connection &
ConnectionManager::add_client(ConnectionParams params)
{
    auto connection_ptr = std::unique_ptr<Connection>(new Connection(*this, params, connection_buffer_size));
    auto &connection = *connection_ptr;
    std::lock_guard<std::mutex> guard(mutex);
    if (connection.connect()) {
        active_connections.push_back(std::move(connection_ptr));
    } else {
        reconnect_connections.push_back(std::move(connection_ptr));
    }
    return connection;
}

void
ConnectionManager::send_loop()
{
    int max_fd;

    fd_set pipe_fds;
    FD_ZERO(&pipe_fds);
    FD_SET(pipe.get_readfd(), &pipe_fds);
    
    fd_set socket_fds;
    FD_ZERO(&socket_fds);

    auto watch_sock = [&](int fd) {
        FD_SET(fd, &pipe_fds);
        max_fd = std::max(max_fd, fd);
    };

    while (!exit_flag) {
        max_fd = pipe.get_readfd();
        FD_ZERO(&socket_fds);

        {
            std::lock_guard<std::mutex> guard(mutex);
            pipe.clear();
            auto it = active_connections.begin();
            while (it != active_connections.end()) {
                auto &connection = **it;
                if (connection.has_data_to_send) {
                    std::lock_guard<std::mutex> guard(connection.buffer_mutex);
                    if (connection.send_some()) {
                        if (connection.buffer.readable()) {
                            watch_sock(connection.sockfd);
                        }
                        connection.has_data_to_send = connection.buffer.readable();
                    } else {
                        connection.connection_lost_flag = true;
                        reconnect_connections.push_back(std::move(*it));
                        it = active_connections.erase(it);
                        reconnect_cv.notify_one();
                        continue;
                    }
                } else {
                    if (connection.close_flag) {
                        it = active_connections.erase(it);
                        continue;
                    }
                }
                it++;
            }
        }

        select(max_fd + 1, &pipe_fds, &socket_fds, NULL, NULL);
    }
}

void
ConnectionManager::reconnect_loop()
{
    while (!exit_flag) {
        auto lock = std::unique_lock<std::mutex>(mutex);
        auto it = reconnect_connections.begin();
        while (it != reconnect_connections.end()) {
            auto &connection = **it;
            if (connection.connect()) {
                active_connections.push_back(std::move(*it));
                it = reconnect_connections.erase(it);
                pipe.notify();
            } else {
                if (connection.close_flag) {
                    it = reconnect_connections.erase(it);
                    continue;
                }
                it++;
            }
        }

        if (reconnect_connections.empty()) {
            reconnect_cv.wait(lock);
        } else {
            reconnect_cv.wait_for(lock, std::chrono::seconds(reconnect_interval_secs));
        }
    }
}

void 
ConnectionManager::start()
{
    send_thread = std::thread([this]() { send_loop(); });
    reconnect_thread = std::thread([this]() { reconnect_loop(); });
}

void 
ConnectionManager::stop()
{
    exit_flag = true;
    pipe.notify();
    reconnect_cv.notify_one();
    send_thread.join();
    reconnect_thread.join();
}

void
ConnectionManager::set_reconnect_interval(int number_of_seconds)
{
    reconnect_interval_secs = number_of_seconds;
}

void
ConnectionManager::set_connection_buffer_size(long number_of_bytes)
{
    connection_buffer_size = number_of_bytes;
}

