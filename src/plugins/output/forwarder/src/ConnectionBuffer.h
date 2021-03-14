/**
 * \file src/plugins/output/forwarder/src/ConnectionBuffer.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Ring buffer used by connections
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

#include <sys/socket.h>

#include <vector>
#include <cstring>
#include <cerrno>
#include <cstdint>

class ConnectionBuffer
{
public:
    ConnectionBuffer(long capacity)
    : capacity(capacity)
    , buffer(capacity)
    {}

    void 
    rollback()
    {
        write_offset = read_end_offset;
    }

    void 
    commit()
    {
        read_end_offset = write_offset;
    }

    long
    writeable()
    {
        return writeable_from(write_offset);
    }

    bool 
    write(uint8_t *data, long length)
    {
        long pos = raw_write_at(write_offset, data, length);
        if (pos == -1) {
            return false;
        }
        write_offset = pos;
        return true;
    }

    template <typename T>
    bool 
    write(T data)
    {
        return write((uint8_t *)&data, sizeof(T));
    }

    long
    readable()
    {
        return read_offset > read_end_offset
            ? capacity - read_offset + read_end_offset
            : read_end_offset - read_offset;
    }

    bool
    peek(uint8_t *data, long length)
    {
        if (readable() < length) {
            return false;
        }
        raw_read_at(read_offset, data, length);
        return true;
    }

    template <typename T>
    bool
    peek(T &item)
    {
        return peek((uint8_t *)&item, sizeof(item));
    }

    int 
    send_data(int sockfd, long length = -1)
    {
        if (length == -1) {
            length = readable();
        }
        iovec iov[2] = {};
        iov[0].iov_len = std::min<std::size_t>(cont_readable_from(read_offset), length);
        iov[0].iov_base = &buffer[read_offset];
        iov[1].iov_len = length - iov[0].iov_len;
        iov[1].iov_base = &buffer[0];
        msghdr msg_hdr = {};
        msg_hdr.msg_iov = iov;
        msg_hdr.msg_iovlen = 2;
        int ret = sendmsg(sockfd, &msg_hdr, MSG_DONTWAIT | MSG_NOSIGNAL);
        if (ret < 0) {
            return (errno == EWOULDBLOCK || errno == EAGAIN) ? 0 : ret;
        }
        read_offset = advance(read_offset, ret);
        return ret;
    }

private:
    long capacity;
    long read_offset = 0;
    long read_end_offset = 0;
    long write_offset = 0;
    std::vector<uint8_t> buffer;

    long
    advance(long pos, long n)
    {
        return (pos + n) % capacity;
    }

    long
    readable_from(long pos)
    {
        return pos > read_end_offset
            ? capacity - pos + read_end_offset
            : read_end_offset - pos;
    }

    long 
    cont_readable_from(long pos)
    {
        return pos > read_end_offset
            ? capacity - pos
            : read_end_offset - pos;
    }

    long
    raw_read_at(long pos, uint8_t *data, long length)
    {
        if (readable_from(pos) < length) {
            return -1;
        }
        long read1 = std::min(cont_readable_from(pos), length);
        long read2 = length - read1;
        memcpy(&data[0], &buffer[pos], read1);
        memcpy(&data[read1], &buffer[advance(pos, read1)], read2);
        return advance(pos, length);
    }

    long
    cont_writeable_from(long pos)
    {
        return read_offset > pos
            ? read_offset - pos - 1
            : (read_offset == 0 ? capacity - pos - 1 : capacity - pos);
    }

    long 
    writeable_from(long pos)
    {
        return read_offset > pos
            ? read_offset - pos - 1
            : capacity - pos + read_offset - 1;
    }

    long
    raw_write_at(long pos, uint8_t *data, long length)
    {
        /// WARNING: Does not advance the write offset
        if (writeable_from(pos) < length) {
            return -1;
        }
        long write1 = std::min(length, cont_writeable_from(pos));
        long write2 = length - write1;
        memcpy(&buffer[pos], &data[0], write1);
        memcpy(&buffer[advance(pos, write1)], &data[write1], write2);
        return advance(pos, length);
    }

    template <typename T>
    bool 
    raw_write_at(long pos, T data)
    {
        return raw_write_at(pos, (uint8_t *)&data, sizeof(T));
    }
    
};