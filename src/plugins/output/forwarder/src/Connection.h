/**
 * \file src/plugins/output/forwarder/src/Connection.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Buffered socket connection
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

#include "ConnectionManager.h"
#include "ConnectionParams.h"
#include "ConnectionBuffer.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <atomic>
#include <mutex>
#include <cstdint>

class ConnectionManager;

class Connection
{
friend class ConnectionManager;

public:
    /// Flag indicating that the connection was lost and the forwarder needs to resend templates etc.
    /// The flag won't be reset when the connection is reestablished!
    std::atomic<bool> connection_lost_flag { false };

    Connection(ConnectionManager &manager, ConnectionParams params, long buffer_size);
    
    bool
    connect();

    std::unique_lock<std::mutex> 
    begin_write();

    bool 
    write(void *data, long length);

    bool
    send_some();

    void 
    commit_write();

    void 
    rollback_write();

    long
    writeable();

    void 
    close();

    ~Connection();

private:
    /// The manager managing this connection
    ConnectionManager &manager;

    /// The parameters to estabilish the connection
    ConnectionParams params;

    /// The connection socket
    int sockfd = -1;

    /// Buffer for the data to send and a mutex guarding it
    /// (buffer will be accessed from sender thread and writer thread)
    std::mutex buffer_mutex;
    ConnectionBuffer buffer;

    /// Flag indicating whether the buffer has any data to send so we don't have to lock the mutex every time
    /// (doesn't need to be atomic because we only set it while holding the mutex)
    bool has_data_to_send = false; 

    /// Flag indicating that the connection has been closed and can be disposed of after the data is sent
    std::atomic<bool> close_flag { false };
};