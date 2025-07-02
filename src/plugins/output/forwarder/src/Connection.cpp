/**
 * \file src/plugins/output/forwarder/src/Connection.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Connection class implementation
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

#include <sys/socket.h>
#include "Connection.h"

#include <stdexcept>
#include <algorithm>
#include <cinttypes>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <cassert>

#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <libfds.h>

#include "Message.h"

Connection::Connection(const std::string &ident, ConnectionParams con_params, ipx_ctx_t *log_ctx,
                       unsigned int tmplts_resend_pkts, unsigned int tmplts_resend_secs,
                       Connector &connector) :
    m_ident(ident),
    m_con_params(con_params),
    m_log_ctx(log_ctx),
    m_tmplts_resend_pkts(tmplts_resend_pkts),
    m_tmplts_resend_secs(tmplts_resend_secs),
    m_connector(connector)
{
}

void
Connection::connect()
{
    assert(m_sockfd.get() < 0);
    m_future_socket = m_connector.get(m_con_params);
}

void
Connection::forward_message(ipx_msg_ipfix_t *msg)
{
    assert(check_connected());

    Sender &sender = get_or_create_sender(msg);
    try {
        sender.process_message(msg);

    } catch (const ConnectionError &err) {
        on_connection_lost();
        throw err;
    }
}

void
Connection::lose_message(ipx_msg_ipfix_t *msg)
{
    Sender &sender = get_or_create_sender(msg);
    sender.lose_message(msg);
}

void
Connection::advance_transfers()
{
    assert(check_connected());

    IPX_CTX_DEBUG(m_log_ctx, "Waiting transfers on connection %s: %zu", m_ident.c_str(), m_transfers.size());

    try {
        for (auto it = m_transfers.begin(); it != m_transfers.end(); ) {

            Transfer &transfer = *it;

            assert(transfer.data.size() <= UINT16_MAX); // The transfer consists of one IPFIX message which cannot be larger

            ssize_t ret = send(m_sockfd.get(), &transfer.data[transfer.offset],
                               transfer.data.size() - transfer.offset, MSG_DONTWAIT | MSG_NOSIGNAL);

            check_socket_error(ret);

            size_t sent = std::max<ssize_t>(0, ret);
            IPX_CTX_DEBUG(m_log_ctx, "Sent %zu/%zu B to %s", sent, transfer.data.size(), m_ident.c_str());

            // Is the transfer done?
            if (transfer.offset + sent == transfer.data.size()) {
                it = m_transfers.erase(it);
                // Remove the transfer and continue with the next one

            } else {
                transfer.offset += sent;

                // Finish, cannot advance next transfer before the one before it is fully sent
                break;
            }
        }
    } catch (ConnectionError& err) {
        on_connection_lost();
        throw err;
    }
}

bool
Connection::check_connected()
{
    if (m_sockfd.get() >= 0) {
        return true;
    }

    if (m_future_socket && m_future_socket->ready()) {
        m_sockfd = m_future_socket->retrieve();
        m_future_socket = nullptr;
        return true;
    }

    return false;
}


static Transfer
make_transfer(const std::vector<iovec> &parts, uint16_t offset, uint16_t total_length)
{
    uint16_t length = total_length - offset;

    // Find first unfinished part
    size_t i = 0;

    while (offset >= parts[i].iov_len) {
        offset -= parts[i].iov_len;
        i++;
    }

    // Copy the unfinished portion
    std::vector<uint8_t> buffer(length); //NOTE: We might want to do this more effectively...
    uint16_t buffer_pos = 0;

    for (; i < parts.size(); i++) {
        memcpy(buffer.data() + buffer_pos, &((uint8_t *) parts[i].iov_base)[offset], parts[i].iov_len - offset);
        buffer_pos += parts[i].iov_len - offset;
        offset = 0;
    }

    // Create the transfer
    Transfer transfer;
    transfer.data = std::move(buffer);
    transfer.offset = 0;

    return transfer;
}

void
Connection::store_unfinished_transfer(Message &msg, uint16_t offset)
{
    Transfer transfer = make_transfer(msg.parts(), offset, msg.length());

    IPX_CTX_DEBUG(m_log_ctx, "Storing unfinished transfer of %" PRIu16 " bytes in connection to %s",
                  msg.length() - offset, m_ident.c_str());

    m_transfers.push_back(std::move(transfer));
}


void
Connection::send_message(Message &msg)
{
    // All waiting transfers have to be sent first
    if (!m_transfers.empty()) {
        store_unfinished_transfer(msg, 0);
        return;
    }

    std::vector<iovec> &parts = msg.parts();

    msghdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.msg_iov = parts.data();
    hdr.msg_iovlen = parts.size();

    ssize_t ret = sendmsg(m_sockfd.get(), &hdr, MSG_DONTWAIT | MSG_NOSIGNAL);

    check_socket_error(ret);

    size_t sent = std::max<ssize_t>(0, ret);

    IPX_CTX_DEBUG(m_log_ctx, "Sent %zu/%" PRIu16 " B to %s", sent, msg.length(), m_ident.c_str());

    if (sent < msg.length()) {
        store_unfinished_transfer(msg, sent);
    }
}

Sender &
Connection::get_or_create_sender(ipx_msg_ipfix_t *msg)
{
    uint32_t odid = ipx_msg_ipfix_get_ctx(msg)->odid;

    if (m_senders.find(odid) == m_senders.end()) {
        m_senders.emplace(odid,
            std::unique_ptr<Sender>(new Sender(
                [&](Message &msg) {
                    send_message(msg);
                },
                m_con_params.protocol == Protocol::TCP,
                m_con_params.protocol != Protocol::TCP ? m_tmplts_resend_pkts : 0,
                m_con_params.protocol != Protocol::TCP ? m_tmplts_resend_secs : 0)));
    }

    Sender &sender = *m_senders[odid].get();

    return sender;
}

void
Connection::check_socket_error(ssize_t sock_ret)
{
    if (sock_ret < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
        char *errbuf;
        ipx_strerror(errno, errbuf);

        IPX_CTX_ERROR(m_log_ctx, "A connection to %s lost! (%s)", m_ident.c_str(), errbuf);
        m_sockfd.reset();

        // All state from the previous connection is lost once new one is estabilished
        m_transfers.clear();

        throw ConnectionError(errbuf);
    }
}

void
Connection::on_connection_lost()
{
    for (auto& p : m_senders) {
        // In case connection was lost, we have to resend templates when it reconnects
        Sender& sender = *p.second.get();
        sender.clear_templates();
    }

    // Do not continue any of the transfers that haven't been finished so we don't end up in the middle of a message
    m_transfers.clear();
}
