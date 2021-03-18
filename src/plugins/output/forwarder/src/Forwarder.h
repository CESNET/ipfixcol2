/**
 * \file src/plugins/output/forwarder/src/Forwarder.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Forwarder header file
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

#include "ForwarderConfig.h"
#include <ipfixcol2.h>
#include <unordered_map>
#include <memory>

constexpr unsigned MAX_TMPLT_MSG_SIZE = 2500;

/// State stored per ODID
struct odid_s {
    /// The most recent templates snapshot, to detect template changed
    const fds_tsnapshot_t *tmplt_snap;
    /// The time templates were last sent, used to check for resend when UDP is used
    time_t last_tmplts_sent_time;
    /// The number of packets of this ODID since the templates were sent, used to check for resend when UDP is used
    uint32_t pkts_since_tmplts_sent;
};

struct host_s;

struct connection_s {
    /// The host this connection is for
    host_s *host;
    /// The connection socket
    int sockfd;
    /// State stored per ODID
    std::unordered_map<uint32_t, odid_s> odids;
    /// The last IPFIX sequence number
    uint32_t seq_num;
    /// Number of transfers the connection is still part of, to avoid prematurely destroying the connection
    unsigned n_transfers;
    /// Flag indicating that the connection is finished and can be destroyed once all the transfers are finished
    bool finished;
};

/// A struct for storing unfinished transfers
struct transfer_s {
    /// The bytes to be sent
    std::vector<uint8_t> data;
    /// The connection to send the data over
    connection_s *connection;
    /// The offset to read the data from, respectively how much was sent already
    uint16_t offset;
};

/// The host parameters and state
struct host_s {
    /// Address of the host, as a hostname or IP address
    std::string address;
    /// Port of the host
    uint16_t port;
    /// Connections to the host per session
    std::unordered_map<const ipx_session *, connection_s *> connections;
};

/// The forwarder parameters and state
class Forwarder {
public:
    /**
     * \brief Initialize a forwarder instance based on the configuration provided
     * \param config   The forwarder configuration
     * \param log_ctx  The logging context
     */
    Forwarder(ForwarderConfig &config, ipx_ctx_t *log_ctx);

    /**
     * \brief Process a session message from the collector and open or close connections
     * \param msg  The session message
     */
    void
    handle_session_message(ipx_msg_session_t *msg);

    /**
     * \brief Process an IPFIX message from the collector and forward it
     * \param fwd  The forwarder instance
     * \param msg  The session message
     */
    void
    handle_ipfix_message(ipx_msg_ipfix_t *msg);

    /**
     * \brief Finish all the waiting transfers and close the connections
     * \param fwd  The forwarder instance
     */
    void
    finalize();

private:
    friend bool
    send_templates_tsnap_for_cb(const fds_template *tmplt, void *data);

    /// The forwarding mode
    forwardmode_e m_forward_mode;
    /// The transport protocol
    protocol_e m_protocol;
    /// How often should we try to reconnect
    unsigned m_reconnect_secs;
    /// How often should we resend templates when UDP is used in seconds
    unsigned m_tmplts_resend_secs;
    /// How often should we resend templates when UDP is used in number of packets processed per ODID
    unsigned m_tmplts_resend_pkts;

    /// The hosts to forward messages to
    std::vector<host_s> m_hosts;
    /// Connections that are down and are waiting to be reconnected
    std::vector<connection_s *> m_reconnects;
    /// A list of transfers that couldn't have been completed because the socket would block
    std::vector<transfer_s> m_waiting_transfers;
    /// Time of the last attempt at reconnecting disconnected connections
    time_t m_last_reconnect_check = 0;
    /// The index of the next host in round-robin mode
    unsigned m_rr_host = 0;
    /// The logging context
    ipx_ctx_t *m_log_ctx;

    void
    process_transfers();

    void
    process_reconnects();

    int
    forward_message(host_s *host, ipx_msg_ipfix_t *msg);

    int
    send_ipfix_message(connection_s *conn, const uint8_t *data, uint16_t len);

    int
    send_templates(connection_s *conn, const fds_ipfix_msg_hdr *hdr, const fds_tsnapshot_t *snap);

    void
    setup_connection(host_s *host, const ipx_session *session);

    void
    close_connection(connection_s *conn);
};

