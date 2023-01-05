/**
 * \file
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Syslog connection (header file)
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

#ifndef JSON_SYSLOG_SOCKET_H
#define JSON_SYSLOG_SOCKET_H

#include <cstdint>
#include <string>

/**
 * \brief Syslog connection type.
 */
enum class SyslogType {
    STREAM,
    DATAGRAM,
};

/** \brief Base class of syslog connection. */
class SyslogSocket {
public:
    SyslogSocket() = default;
    virtual ~SyslogSocket();

    SyslogSocket(const SyslogSocket &other) = delete;
    SyslogSocket& operator=(const SyslogSocket &other) = delete;
    SyslogSocket(SyslogSocket &&other) = delete;
    SyslogSocket& operator=(SyslogSocket &&other) = delete;

    /**
     * \brief Test if the socket is open and ready.
     */
    bool is_ready() const noexcept { return m_fd >= 0; };
    /**
     * \brief Get connection type.
     */
    virtual SyslogType type() const noexcept = 0;
    /**
     * \brief Open socket and connect to the syslog
     * \return 0 on success. Otherwise returns a negative errno-like code.
     */
    virtual int open() = 0;
    /**
     * \brief Close socket.
     * \note No action is performed, if the socket is already closed.
     */
    void close() noexcept;
    /**
     * \brief Write a message to the syslog socket.
     *
     * The function might change \p msg by updating msg_iov, iov_base and iov_len
     * variables.
     *
     * \param[in] msg Vector of message parts to send.
     * \return 1 if the message has been send (might be still partly stored in buffer);
     * \return 0 if the connection would block and the message cannot be sent.
     * \return a negative errno-like code if the connection is broken.
     */
    virtual int write(struct msghdr *msg) = 0;
    /**
     * \brief Get connection description (for logging)
     */
    virtual std::string description() = 0;

protected:
    int m_fd = -1;
};

/** @brief Syslog connection over a TCP socket. */
class TcpSyslogSocket : public SyslogSocket {
public:
    TcpSyslogSocket(const std::string &hostname, uint16_t port, bool blocking);
    ~TcpSyslogSocket() = default;

    SyslogType type() const noexcept override { return SyslogType::STREAM; };
    int open() override;
    int write(struct msghdr *msg) override;
    std::string description() override;

private:
    std::string m_hostname;
    uint16_t m_port;
    std::string m_buffer;
    bool m_blocking;
};

/** @brief Syslog connection over a UDP socket. */
class UdpSyslogSocket : public SyslogSocket {
public:
    UdpSyslogSocket(const std::string &hostname, uint16_t port);
    ~UdpSyslogSocket() = default;

    SyslogType type() const noexcept override { return SyslogType::DATAGRAM; };
    int open() override;
    int write(struct msghdr *msg) override;
    std::string description() override;

private:
    std::string m_hostname;
    uint16_t m_port;
};

#endif // JSON_SYSLOG_SOCKET_H
