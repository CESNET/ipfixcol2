/**
 * \file src/plugins/output/forwarder/src/Connection.cpp
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

#include "Connection.h"

#include <libfds.h>

Connection::Connection(ConnectionManager &manager, ConnectionParams params, long buffer_size)
: manager(manager)
, params(params)
, buffer(buffer_size)
{
}

bool
Connection::connect()
{
    if (sockfd >= 0) {
        ::close(sockfd);
    }
    sockfd = params.make_socket();
    return sockfd >= 0;
}

std::unique_lock<std::mutex> 
Connection::begin_write()
{
    return std::unique_lock<std::mutex>(buffer_mutex);
}

bool 
Connection::write(void *data, long length)
{
    return buffer.write((uint8_t *)data, length);
}

void 
Connection::rollback_write()
{
    buffer.rollback();
}

long
Connection::writeable()
{
    return buffer.writeable();
}

void 
Connection::commit_write()
{
    buffer.commit();
    manager.pipe.notify();
    has_data_to_send = buffer.readable();
}

bool
Connection::send_some()
{
    if (params.protocol == TransProto::Udp) {
        while (1) {
            fds_ipfix_msg_hdr ipfix_header;
            if (!buffer.peek(ipfix_header)) {
                return true;
            }
            auto message_length = ntohs(ipfix_header.length);
            int ret = buffer.send_data(sockfd, message_length);
            if (ret == 0 || !buffer.readable()) {
                return true;
            } else if (ret < 0) {
                return false;
            }
        }
        return true;
    } else {
        return buffer.send_data(sockfd) >= 0;
    }
}

void 
Connection::close()
{
    close_flag = true;
    manager.pipe.notify();
}

Connection::~Connection()
{
    if (sockfd >= 0) {
        ::close(sockfd);
    }
}