/**
 * \file src/plugins/output/forwarder/src/ConnectionManager.h
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

#pragma once

#include "ConnectionParams.h"
#include "Connection.h"
#include "SyncPipe.h"

#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <thread>
#include <cstdint>

class Connection;

static constexpr long DEFAULT_BUFFER_SIZE = 4 * 1024 * 1024;
static constexpr int DEFAULT_RECONNECT_INTERVAL_SECS = 5;

class ConnectionManager
{
friend class Connection;

public:
    Connection &
    add_client(ConnectionParams params);

    void
    start();

    void 
    stop();

    void
    set_reconnect_interval(int number_of_seconds);

    void
    set_connection_buffer_size(long number_of_bytes);
    
private:
    long connection_buffer_size = DEFAULT_BUFFER_SIZE;
    int reconnect_interval_secs = DEFAULT_RECONNECT_INTERVAL_SECS;
    std::mutex mutex;
    std::vector<std::unique_ptr<Connection>> active_connections;
    std::vector<std::unique_ptr<Connection>> reconnect_connections;
    std::thread send_thread;
    std::thread reconnect_thread;
    std::condition_variable reconnect_cv;
    std::atomic<bool> exit_flag { false };
    SyncPipe pipe;

    void
    send_loop();

    void 
    reconnect_loop();
};
