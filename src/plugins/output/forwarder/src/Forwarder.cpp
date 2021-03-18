/**
 * \file src/plugins/output/forwarder/src/Forwarder.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Forwarder implementation file
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
#include "Forwarder.h"
#include <netdb.h>
#include <unistd.h>
#include <algorithm>
#include <cstring>
#include <cinttypes>

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Extra functions

/**
 * Create and connect a socket
 * \param address   The host address, hostname or IP address
 * \param port      The port number
 * \param protocol  The protocol
 * \return The socket file descriptor
 */
static int
make_socket(const std::string &address, uint16_t port, protocol_e protocol)
{
    addrinfo *ai;
    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;

    switch (protocol) {
    case protocol_e::TCP:
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_socktype = SOCK_STREAM;
        break;

    case protocol_e::UDP:
        hints.ai_protocol = IPPROTO_UDP;
        hints.ai_socktype = SOCK_DGRAM;
        break;

    default: assert(0);
    }

    if (getaddrinfo(address.c_str(), std::to_string(port).c_str(), &hints, &ai) != 0) {
        return -1;
    }

    int sockfd;
    addrinfo *p;
    for (p = ai; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) {
            continue;
        }

        if (protocol == protocol_e::UDP) {
            sockaddr_in sa = {};
            sa.sin_family = AF_INET;
            sa.sin_port = 0;
            sa.sin_addr.s_addr = INADDR_ANY;
            sa.sin_port = 0;

            if (bind(sockfd, (sockaddr *)&sa, sizeof(sa)) != 0) {
                close(sockfd);
                continue;
            }
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) != 0) {
            close(sockfd);
            continue;
        }
        break;
    }

    freeaddrinfo(ai);

    if (!p) {
        return -1;
    }

    return sockfd;
}

/// A context holder for the send_templates_tsnap_for_cb
struct send_templates_aux_s {
    Forwarder *fwd;
    connection_s *conn;
    std::vector<uint8_t> buf;
    uint16_t offset;
    uint16_t sethdr_offset;
    bool error;
};

/**
 * \brief A callback for the fds_tsnapshot_for used in send_templates, cannot throw exceptions!
 *        Writes one template to the prepared template message and sends the message if it's already too large
 *
 * \param tmplt  The template to be written
 * \param data   The context in form of a send_templates_aux_s struct
 * \return true if everything went well for the iteration to continue, false if an error occured and we have to abort
 */
bool
send_templates_tsnap_for_cb(const fds_template *tmplt, void *data)
{
    send_templates_aux_s *aux = (send_templates_aux_s *) data;
    fds_ipfix_msg_hdr *hdr = (fds_ipfix_msg_hdr *) &aux->buf[0];
    fds_ipfix_set_hdr *sethdr = (fds_ipfix_set_hdr *) &aux->buf[aux->sethdr_offset]; // Might be invalid but we'll check before accessing
    uint16_t setid = ntohs(tmplt->type == FDS_TYPE_TEMPLATE ? FDS_IPFIX_SET_TMPLT : FDS_IPFIX_SET_OPTS_TMPLT);
    uint16_t len = tmplt->raw.length;

    // Calculate additional space required
    unsigned space_needed = len;
    if (aux->sethdr_offset == 0 || sethdr->flowset_id != setid) {
        space_needed += sizeof(fds_ipfix_set_hdr);
    }


    // If the message would be too long with the additional template written, send the current
    // message and start a new one
    if (aux->offset + space_needed > MAX_TMPLT_MSG_SIZE) {
        sethdr->length = htons(sethdr->length);
        hdr->length = htons(hdr->length);

        try {
            if (!aux->fwd->send_ipfix_message(aux->conn, &aux->buf[0], aux->offset)) {
                aux->error = true;
                return false;
            }

        } catch (std::bad_alloc &) {
            // Allocation error could happen and we're in a C callback function that cannot
            // propagate exceptions, we have to catch it here
            aux->error = true;
            return false;
        }

        // Keep the message header and only reset the length
        aux->offset = sizeof(fds_ipfix_msg_hdr);
        hdr->length = sizeof(fds_ipfix_msg_hdr);
        aux->sethdr_offset = 0;
    }

    // Make sure buffer has enough space
    if (aux->offset + space_needed > aux->buf.size()) {
        try {
            aux->buf.resize((aux->offset + space_needed + 1023) / 1024 * 1024); // Round up to nearest multiply of 1024
        } catch (std::bad_alloc &) {
            aux->error = true;
            return false;
        }
        hdr = (fds_ipfix_msg_hdr *) &aux->buf[0];
        sethdr = (fds_ipfix_set_hdr *) &aux->buf[aux->sethdr_offset];
    }

    // Create new flowset if one hasn't been created yet or if the template is of a different
    // type than the current flowset
    if (aux->sethdr_offset == 0 || sethdr->flowset_id != setid) {
        if (aux->sethdr_offset != 0) { // Finalize the previous set if there is one
            sethdr->length = htons(sethdr->length);
        }
        aux->sethdr_offset = aux->offset;
        aux->offset += sizeof(fds_ipfix_set_hdr);
        hdr->length += sizeof(fds_ipfix_set_hdr);
        sethdr = (fds_ipfix_set_hdr *) &aux->buf[aux->sethdr_offset];
        sethdr->flowset_id = setid;
        sethdr->length = sizeof(fds_ipfix_set_hdr);
    }

    // Finally write the template
    memcpy(&aux->buf[aux->offset], tmplt->raw.data, len);
    aux->offset += len;
    hdr->length += len;
    sethdr->length += len;
    return true;
}

/**
 * \brief Make a copy of the IPFIX message without the template sets
 * \param msg  The original IPFIX message
 * \return Copy of the IPFIX message without the template sets
 */
static std::vector<uint8_t>
copy_strip_template_sets(ipx_msg_ipfix_t *msg)
{
    fds_ipfix_msg_hdr *msg_hdr = (fds_ipfix_msg_hdr *) ipx_msg_ipfix_get_packet(msg);
    uint16_t msg_len = ntohs(msg_hdr->length);

    // Strip templates from the IPFIX message
    ipx_ipfix_set *sets;
    size_t num_sets;

    // Message won't be longer than the original message
    std::vector<uint8_t> buffer(msg_len);
    uint16_t offset = 0;

    // Copy the original message header
    fds_ipfix_msg_hdr *new_msg_hdr = (fds_ipfix_msg_hdr *) &buffer[0];
    std::memcpy(&buffer[0], msg_hdr, sizeof(fds_ipfix_msg_hdr));
    offset += sizeof(fds_ipfix_msg_hdr);

    // Skip template sets, copy over other sets
    ipx_msg_ipfix_get_sets(msg, &sets, &num_sets);
    for (size_t i = 0; i < num_sets; i++) {
        uint16_t set_id = ntohs(sets[i].ptr->flowset_id);
        if (set_id == FDS_IPFIX_SET_TMPLT || set_id == FDS_IPFIX_SET_OPTS_TMPLT) {
            continue;
        }

        uint16_t set_len = ntohs(sets[i].ptr->length);
        std::memcpy(&buffer[offset], sets[i].ptr, set_len);
        offset += set_len;
    }

    // Finish the message header
    new_msg_hdr->length = htons(offset);

    // To pass the message length
    buffer.resize(offset);

    return buffer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Public methods

/**
 * \brief Initialize a forwarder instance based on the configuration provided
 * \param config   The forwarder configuration
 * \param log_ctx  The logging context
 */
Forwarder::Forwarder(ForwarderConfig &config, ipx_ctx_t *log_ctx)
{
    m_log_ctx = log_ctx;
    m_protocol = config.protocol;
    m_forward_mode = config.forward_mode;
    m_reconnect_secs = config.reconnect_secs;
    m_tmplts_resend_pkts = config.tmplts_resend_pkts;
    m_tmplts_resend_secs = config.tmplts_resend_secs;

    for (const auto &hostinfo : config.hosts) {
        host_s host = {};
        host.address = hostinfo.address;
        host.port = hostinfo.port;
        m_hosts.push_back(std::move(host));
    }
}

/**
 * \brief Process a session message from the collector and open or close connections
 * \param msg  The session message
 */
void
Forwarder::handle_session_message(ipx_msg_session_t *msg)
{
    const ipx_session *session = ipx_msg_session_get_session(msg);

    switch (ipx_msg_session_get_event(msg)) {
    case IPX_MSG_SESSION_OPEN:
        IPX_CTX_DEBUG(m_log_ctx, "New session %s", session->ident);
        for (host_s &host : m_hosts) {
            setup_connection(&host, session);
        }
        break;

    case IPX_MSG_SESSION_CLOSE:
        IPX_CTX_DEBUG(m_log_ctx, "Closing session %s", session->ident);
        for (host_s &host : m_hosts) {
            connection_s *conn = host.connections[session];
            host.connections.erase(session);
            close_connection(conn);
        }
        break;
    }
}

/**
 * \brief Process an IPFIX message from the collector and forward it
 * \param fwd  The forwarder instance
 * \param msg  The session message
 */
void
Forwarder::handle_ipfix_message(ipx_msg_ipfix_t *msg)
{
    time_t now = time(NULL);

    // Process reconnects
    if (now - m_last_reconnect_check >= m_reconnect_secs) {
        process_reconnects();
        m_last_reconnect_check = now;
    }

    // Process waiting transfers
    process_transfers();

    // Process current message
    switch (m_forward_mode) {
    case forwardmode_e::SENDTOALL:
        for (host_s &host : m_hosts) {
            forward_message(&host, msg);
        }
        break;

    case forwardmode_e::ROUNDROBIN:
        for (size_t i = 0; i < m_hosts.size(); i++) {
            int rc = forward_message(&m_hosts[m_rr_host], msg);
            m_rr_host = (m_rr_host + 1) % m_hosts.size();
            if (rc == 1) {
                break;
            }
        }
        break;

    default: assert(0);
    }
}

/**
 * \brief Finish all the waiting transfers and close the connections
 * \param fwd  The forwarder instance
 */
void
Forwarder::finalize()
{
    IPX_CTX_DEBUG(m_log_ctx, "Finalizing");

    for (host_s &host : m_hosts) {
        for (auto &p : host.connections) {
            close_connection(p.second);
        }
    }

    while (m_waiting_transfers.size() > 0) {
        process_reconnects();
        process_transfers();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Private methods

/**
 * \brief Setup a connection with a host for a new session
 * \param fwd      The forwarder instance
 * \param host     The host instance
 * \param session  The collector session
 */
void
Forwarder::setup_connection(host_s *host, const ipx_session *session)
{
    connection_s *conn = new connection_s{};
    conn->host = host;
    conn->sockfd = make_socket(host->address, host->port, m_protocol);

    if (conn->sockfd < 0) {
        IPX_CTX_WARNING(m_log_ctx, "Host %s:" PRIu16 " is unreachable",
                        conn->host->address.c_str(), conn->host->port);
        m_reconnects.push_back(conn);
    }

    IPX_CTX_INFO(m_log_ctx, "Opened new connection for %s:" PRIu16, host->address.c_str(), host->port);
    host->connections.emplace(session, conn);
}
/**
 * \brief Close a connection, also destroy it if it's not needed in any of the waiting transfers
 * \param conn  The connection
 */
void
Forwarder::close_connection(connection_s *conn)
{
    if (conn->n_transfers > 0) {
        IPX_CTX_DEBUG(m_log_ctx, "Setting connection %s:" PRIu16 " as finished",
                      conn->host->address.c_str(), conn->host->port);
        conn->finished = true;
        return;
    }

    IPX_CTX_DEBUG(m_log_ctx, "Destroying connection for %s:" PRIu16,
                  conn->host->address.c_str(), conn->host->port);
    m_reconnects.erase(std::remove(m_reconnects.begin(), m_reconnects.end(), conn), m_reconnects.end());
    if (conn->sockfd >= 0) {
        close(conn->sockfd);
    }
    delete conn;
}

/**
 * \brief Send an IPFIX message over connection in a non-blocking way
 *
 * If the connection dropped, add the connection to the reconnection list in the forwarder instance
 * If whole message couldn't be sent, store it as a waiting transfer in the forwarder instance
 *
 * \param conn  The connection
 * \return 1 if the data were sent or stored in the waiting transfers list, 0 if the connection dropped
 */
int
Forwarder::send_ipfix_message(connection_s *conn, const uint8_t *data, uint16_t len)
{
    int sent = send(conn->sockfd, data, len, MSG_DONTWAIT);
    if (sent == len) {
        // Everything was sent, we're done
        return 1;
    }

    if (sent < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
        // Socket disconnected, set up reconnection
        IPX_CTX_WARNING(m_log_ctx, "%s:" PRIu16 " disconnected, planning reconnection",
                        conn->host->address.c_str(), conn->host->port);
        close(conn->sockfd);
        conn->sockfd = -1;
        m_reconnects.push_back(conn);
        return 0;
    }

    // We have to finish the transfer later.
    // If it's TCP only send the remainder, in case of UDP everything has to be resent.
    uint16_t remaining;
    if (m_protocol == protocol_e::TCP && sent > 0) {
        remaining = len - sent;
    } else {
        remaining = len;
    }

    transfer_s transfer;
    transfer.offset = 0;
    transfer.connection = conn;
    transfer.data.resize(remaining);
    memcpy(&transfer.data[0], &data[len - remaining], remaining);

    conn->n_transfers++;

    m_waiting_transfers.push_back(std::move(transfer));

    return 1;
}

/**
 * \brief Send all the templates from the snapshot over the connection
 * \param conn  The connection
 * \param hdr   The IPFIX message header used as a template for the template message header
 * \param snap  The templates snapshot
 * \return 1 on success, 0 if the connection dropped or an error occured while building the message
 */
int
Forwarder::send_templates(connection_s *conn, const fds_ipfix_msg_hdr *hdr, const fds_tsnapshot_t *snap)
{
    send_templates_aux_s aux = {};
    aux.offset = 0;
    aux.fwd = this;
    aux.conn = conn;
    aux.buf.resize(1024);
    aux.sethdr_offset = 0;

    fds_ipfix_msg_hdr *tmpltmsg_hdr = (fds_ipfix_msg_hdr *)&aux.buf[0];
    memcpy(tmpltmsg_hdr, hdr, sizeof(fds_ipfix_msg_hdr));
    tmpltmsg_hdr->length = sizeof(fds_ipfix_msg_hdr);
    aux.offset += sizeof(fds_ipfix_msg_hdr);

    if (m_protocol == protocol_e::TCP) {
        uint16_t wdrl_all[] = {
            htons(2), htons(8), // Template Withdrawal All
            htons(2), htons(0),
            htons(3), htons(8), // Option Template Withdrawal All
            htons(3), htons(0)
        };
        memcpy(&aux.buf[aux.offset], wdrl_all, sizeof(wdrl_all));
        aux.offset += sizeof(wdrl_all);
        tmpltmsg_hdr->length += sizeof(wdrl_all);
    }

    fds_tsnapshot_for(snap, send_templates_tsnap_for_cb, &aux);
    if (aux.error) {
        return 0;
    }
    tmpltmsg_hdr = (fds_ipfix_msg_hdr *) &aux.buf[0];
    fds_ipfix_set_hdr *sethdr = (fds_ipfix_set_hdr *) &aux.buf[aux.sethdr_offset];
    if (aux.sethdr_offset != 0) {
        sethdr->length = htons(sethdr->length);
        tmpltmsg_hdr->length = htons(tmpltmsg_hdr->length);
        if (!send_ipfix_message(conn, &aux.buf[0], aux.offset)) {
            return 0;
        }
    }
    return 1;
}

/**
 * \brief Forward an IPFIX message to the host, also handle sending templates if they need to be send
 * \param host  The host instance
 * \param msg   The IPFIX message
 */
int
Forwarder::forward_message(host_s *host, ipx_msg_ipfix_t *msg)
{
    const ipx_session *session = ipx_msg_ipfix_get_ctx(msg)->session;
    uint32_t drec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);

    if (drec_cnt == 0) {
        // Nothing to do, there are no data record to send and we can't get templates snapshot
        return 1;
    }

    uint8_t *msg_data = ipx_msg_ipfix_get_packet(msg);
    fds_ipfix_msg_hdr *msg_hdr = (fds_ipfix_msg_hdr *)msg_data;
    uint16_t msg_len = ntohs(msg_hdr->length);

    // Get the connection for the session and host
    connection_s *conn = host->connections[session];
    if (conn->sockfd < 0) {
        assert(m_protocol != protocol_e::UDP);
        // The socket is down and hasn't reconnected yet, skip this host
        return 0;
    }

    // Get the corresponding ODID struct or create new one
    odid_s *odid;
    auto odid_it = conn->odids.find(msg_hdr->odid);
    if (odid_it == conn->odids.end()) {
        odid = &conn->odids[msg_hdr->odid];
        *odid = {};
    } else {
        odid = &odid_it->second;
    }

    // Adjust the sequence number for the connection and store the original as we need to put it back later
    uint32_t original_seqnum = msg_hdr->seq_num;
    msg_hdr->seq_num = htonl(conn->seq_num);

    const fds_tsnapshot_t *snap = ipx_msg_ipfix_get_drec(msg, 0)->rec.snap;

    time_t now = time_t(NULL);

    if (odid->tmplt_snap != snap && m_forward_mode != forwardmode_e::SENDTOALL) {

        IPX_CTX_DEBUG(m_log_ctx, "Resending templates because templates changed");

        if (send_templates(conn, msg_hdr, snap) != 1) {
            msg_hdr->seq_num = original_seqnum;
            return 0;
        }
        odid->tmplt_snap = snap;
        odid->pkts_since_tmplts_sent = 0;
        odid->last_tmplts_sent_time = now;

        // Forward the message without the template sets
        std::vector<uint8_t> new_msg = copy_strip_template_sets(msg);
        if (send_ipfix_message(conn, new_msg.data(), new_msg.size()) != 1) {
            msg_hdr->seq_num = original_seqnum;
            return 0;
        }

    } else if (m_protocol == protocol_e::UDP
        && (odid->pkts_since_tmplts_sent >= m_tmplts_resend_pkts || now - odid->pkts_since_tmplts_sent >= m_tmplts_resend_secs)) {

        IPX_CTX_DEBUG(m_log_ctx, "Resending templates because resend interval elapsed");

        if (send_templates(conn, msg_hdr, snap) != 1) {
            msg_hdr->seq_num = original_seqnum;
            return 0;
        }
        odid->tmplt_snap = snap;
        odid->pkts_since_tmplts_sent = 0;
        odid->last_tmplts_sent_time = now;

        // Forward the message as is
        if (send_ipfix_message(conn, msg_data, msg_len) != 1) {
            msg_hdr->seq_num = original_seqnum;
            return 0;
        }

    } else {
        // Forward the message as is
        if (send_ipfix_message(conn, msg_data, msg_len) != 1) {
            msg_hdr->seq_num = original_seqnum;
            return 0;
        }
        odid->pkts_since_tmplts_sent++;
    }

    conn->seq_num += drec_cnt;
    msg_hdr->seq_num = original_seqnum;
    return 1;
}

/**
 * \brief Go through the connections in the reconnections list of the forwarder instance and try to reestabilish connection
 */
void
Forwarder::process_reconnects()
{
    auto it = m_reconnects.begin();
    while (it != m_reconnects.end()) {
        connection_s *conn = *it;
        IPX_CTX_INFO(m_log_ctx, "Trying to reconnect %s:" PRIu16, conn->host->address.c_str(), conn->host->port);

        conn->sockfd = make_socket(conn->host->address, conn->host->port, m_protocol);
        if (conn->sockfd > 0) {
            IPX_CTX_INFO(m_log_ctx, "%s:" PRIu16 " reconnected", conn->host->address.c_str(), conn->host->port);
            it = m_reconnects.erase(it);
        } else {
            it++;
        }
    }
}

/**
 * \brief Go through the waiting transfers list of the forwarder instance and try to send them
 */
void
Forwarder::process_transfers()
{
    IPX_CTX_DEBUG(m_log_ctx, "Waiting transfers: %lu", m_waiting_transfers.size());
    auto it = m_waiting_transfers.begin();
    while (it != m_waiting_transfers.end()) {
        transfer_s &transfer = *it;

        if (transfer.connection->sockfd < 0) {
            it++;
            continue;
        }

        assert(transfer.data.size() <= UINT16_MAX);
        int sent = send(transfer.connection->sockfd, &transfer.data[transfer.offset],
                        transfer.data.size() - transfer.offset, MSG_DONTWAIT);

        if (sent > 0 && (size_t) (transfer.offset + sent) == transfer.data.size()) {
            transfer.connection->n_transfers--;
            if (transfer.connection->n_transfers == 0 && transfer.connection->finished) {
                close_connection(transfer.connection);
            }
            it = m_waiting_transfers.erase(it);
            continue;
        }

        if (sent > 0 && m_protocol == protocol_e::TCP) {
            transfer.offset += sent;
        }

        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            close(transfer.connection->sockfd);
            transfer.connection->sockfd = -1;
            m_reconnects.push_back(transfer.connection);
        }
        it++;
    }
}

