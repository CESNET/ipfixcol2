/**
 * \file
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Syslog connection
 * \date 2023
 */

/* Copyright (C) 2023 CESNET, z.s.p.o.
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

#include <cassert>
#include <cstring>
#include <stdexcept>

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "SyslogSocket.hpp"

static int
inet_socket(const std::string hostname, uint16_t port, int sock_type, int sock_proto)
{
    const std::string port_str = std::to_string(port);
    struct addrinfo *result;
    struct addrinfo hints;
    int ret;
    int fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = sock_type;
    hints.ai_protocol = sock_proto;
    hints.ai_flags = AI_NUMERICSERV;

    ret = getaddrinfo(hostname.c_str(), port_str.c_str(), &hints, &result);
    if (ret != 0) {
        return -EHOSTUNREACH;
    }

    for (struct addrinfo *ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        // Try to create a new socket and connect
        fd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (fd < 0) {
            continue;
        }

        if (::connect(fd, ptr->ai_addr, ptr->ai_addrlen) < 0) {
            ::close(fd);
            fd = -1;
            continue;
        }

        // Success
        break;
    }

    freeaddrinfo(result);
    return (fd < 0) ? -EHOSTUNREACH : fd;
}

static size_t
msghdr_size(const struct msghdr *msg)
{
    size_t sum = 0;

    for (size_t i = 0; i < msg->msg_iovlen; ++i) {
        sum += msg->msg_iov[i].iov_len;
    }

    return sum;
}

static std::string
msghdr_to_string(const struct msghdr *msg, size_t offset = 0)
{
    std::string result;

    for (size_t i = 0; i < msg->msg_iovlen; ++i) {
        const struct iovec *block = &msg->msg_iov[i];
        const char *data = (const char *) block->iov_base;
        size_t data_size = block->iov_len;

        if (offset > 0) {
            if (offset >= data_size) {
                offset -= data_size;
                continue;
            }

            data += offset;
            data_size -= offset;
            offset = 0;
        }

        result.append(data, data_size);
    }

    return result;
}

static void
msghdr_remove_prefix(struct msghdr *msg, size_t offset)
{
    if (offset == 0) {
        return;
    }

    for (size_t i = 0; i < msg->msg_iovlen; ++i) {
        struct iovec *block = &msg->msg_iov[i];
        char *data = (char *) block->iov_base;
        size_t data_size = block->iov_len;

        if (offset >= data_size) {
            offset -= data_size;
            continue;
        }

        // Edit the block
        block->iov_base = &data[offset];
        block->iov_len -= offset;

        // Skip unused blocks
        msg->msg_iov = &msg->msg_iov[i];
        msg->msg_iovlen -= i;
        return;
    }

    assert(false && "offset is out-of-range");
}

static int
__send_stream_nonblocking(int fd, struct msghdr *msg)
{
    size_t remain = msghdr_size(msg);

    while (remain > 0) {
        ssize_t ret = sendmsg(fd, msg, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (ret < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                return 0;
            }

            return -errno;
        }

        assert(remain >= static_cast<size_t>(ret));
        remain -= static_cast<size_t>(ret);

        if (remain > 0) {
            msghdr_remove_prefix(msg, static_cast<size_t>(ret));
        }
    }

    return 1;
}

static int
__send_stream_nonblocking_buffer(int fd, std::string &buffer)
{
    struct iovec iovec;
    struct msghdr tmp;
    int ret;

    if (buffer.empty()) {
        return 1;
    }

    memset(&tmp, 0, sizeof(tmp));

    iovec.iov_base = (void *) buffer.data();
    iovec.iov_len = buffer.size();
    tmp.msg_iov = &iovec;
    tmp.msg_iovlen = 1;

    ret = __send_stream_nonblocking(fd, &tmp);
    if (ret < 0) {
        // An error has occurred
        return ret;
    }

    if (ret == 0) {
        // Unable to send the whole buffer
        const size_t remain = msghdr_size(&tmp);
        const size_t sent = buffer.size() - remain;
        assert(remain > 0 && remain <= buffer.size());

        if (sent != 0) {
            buffer.erase(0, sent);
        }

        return 0;
    }

    buffer.clear();
    return 1;
}

static int
send_stream_nonblocking(int fd, std::string &buffer, struct msghdr *msg)
{
    int ret;

    // Send remaining data (if not empty)
    ret = __send_stream_nonblocking_buffer(fd, buffer);
    if (ret <= 0) {
        return ret;
    }

    // Sent the new message
    ret = __send_stream_nonblocking(fd, msg);
    if (ret < 0) {
        return ret;
    }

    if (ret > 0) {
        // Sucess
        return 1;
    }

    // Store rest to the buffer
    buffer = msghdr_to_string(msg);
    // Mark the message as sent even when it is still in the buffer
    return 1;
}

static int
send_stream_blocking(int fd, struct msghdr *msg)
{
    size_t remain = msghdr_size(msg);

    while (remain > 0) {
        ssize_t ret = sendmsg(fd, msg, MSG_NOSIGNAL);
        if (ret < 0) {
            return -errno;
        }

        assert(remain >= static_cast<size_t>(ret));
        remain -= static_cast<size_t>(ret);
        if (remain > 0) {
            msghdr_remove_prefix(msg, static_cast<size_t>(ret));
        }
    }

    return 1;
}

static int
send_datagram_nonblocking(int fd, struct msghdr *msg)
{
    ssize_t ret = sendmsg(fd, msg, MSG_NOSIGNAL | MSG_DONTWAIT);
    if (ret < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return 0;
        }

        return -errno;
    }

    return 1;
}

SyslogSocket::~SyslogSocket()
{
    close();
}

void SyslogSocket::close() noexcept
{
    if (m_fd < 0) {
        return;
    }

    ::close(m_fd);
    m_fd = -1;
}

TcpSyslogSocket::TcpSyslogSocket(const std::string &hostname, uint16_t port, bool blocking)
    : SyslogSocket()
    , m_hostname(hostname)
    , m_port(port)
    , m_blocking(blocking)
{
}

int
TcpSyslogSocket::open()
{
    int ret;

    m_buffer.clear();
    close();

    ret = inet_socket(m_hostname, m_port, SOCK_STREAM, 0);
    if (ret < 0) {
        return ret;
    }

    m_fd = ret;
    return 0;
}

int
TcpSyslogSocket::write(struct msghdr *msg)
{
    int ret = -EINVAL;

    if (!is_ready()) {
        return ret;
    }

    if (m_blocking) {
        ret = send_stream_blocking(m_fd, msg);
    } else {
        ret = send_stream_nonblocking(m_fd, m_buffer, msg);
    }

    if (ret < 0) {
        close();
    }

    return ret;
}

std::string
TcpSyslogSocket::description()
{
    return m_hostname + ":" + std::to_string(m_port);
}

UdpSyslogSocket::UdpSyslogSocket(const std::string &hostname, uint16_t port)
    : SyslogSocket()
    , m_hostname(hostname)
    , m_port(port)
{
}

int
UdpSyslogSocket::open()
{
    int ret;

    close();

    ret = inet_socket(m_hostname, m_port, SOCK_DGRAM, 0);
    if (ret < 0) {
        return ret;
    }

    m_fd = ret;
    return 0;
}

int
UdpSyslogSocket::write(struct msghdr *msg)
{
    int ret = -EINVAL;

    if (!is_ready()) {
        return ret;
    }

    ret = send_datagram_nonblocking(m_fd, msg);
    if (ret < 0) {
        close();
    }

    return ret;
}

std::string
UdpSyslogSocket::description()
{
    return m_hostname + ":" + std::to_string(m_port);
}
