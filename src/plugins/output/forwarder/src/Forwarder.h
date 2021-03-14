/**
 * \file src/plugins/output/forwarder/src/Forwarder.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Forwarder logic
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
#include "IPFIXMessage.h"
#include "MessageBuilder.h"

#include <ipfixcol2.h>
#include <libfds.h>

#include <map>
#include <memory>

enum class ForwardMode { SendToAll, RoundRobin };

struct Session;
struct Client;

struct Odid
{
    Odid() {} // Default constructor needed because of std::map

    Odid(Session &session, uint32_t odid)
    : session(&session)
    , odid(odid)
    {}

    Session *session;
    uint32_t odid;
    uint32_t seq_num = 0;

    const fds_tsnapshot_t *templates_snapshot = NULL;
    std::time_t last_templates_send_time = 0;
    unsigned bytes_since_templates_sent = 0;

    std::string
    str();

    void
    reset_values()
    {
        seq_num = 0;
        templates_snapshot = NULL;
        last_templates_send_time = 0;
        bytes_since_templates_sent = 0;
    }
};

struct Session
{
    Session(Connection &connection, Client &client, std::string ident)
    : connection(connection)
    , client(client)
    , ident(ident)
    {}

    Connection &connection;
    Client &client;
    std::string ident;

    std::map<uint32_t, Odid> odids;

    std::string
    str();
};

struct Client
{
    Client(ConnectionManager &connection_manager, ConnectionParams connection_params, std::string name = "")
    : connection_manager(connection_manager)
    , connection_params(connection_params)
    , name(name)
    {}

    ConnectionManager &connection_manager;
    ConnectionParams connection_params;
    std::string name;
    std::map<const ipx_session *, std::unique_ptr<Session>> sessions;

    std::string
    str()
    {
        return name;
    }
};

std::string
Odid::str()
{
    return session->ident + "(" + std::to_string(odid) + ") -> " + session->client.name;
}

std::string
Session::str()
{
    return ident + " -> " + client.name;
}

class Forwarder
{
public:
    Forwarder(ipx_ctx_t *log_ctx)
    : log_ctx(log_ctx)
    {}

    void
    set_transport_protocol(TransProto transport_protocol)
    {
        this->transport_protocol = transport_protocol;
    }

    void
    set_forward_mode(ForwardMode forward_mode)
    {
        this->forward_mode = forward_mode;
    }

    void
    set_connection_buffer_size(long number_of_bytes)
    {
        connection_manager.set_connection_buffer_size(number_of_bytes);
    }

    void
    set_template_refresh_interval_secs(int number_of_seconds)
    {
        this->template_refresh_interval_secs = number_of_seconds;
    }

    void
    set_template_refresh_interval_bytes(int number_of_bytes)
    {
        this->template_refresh_interval_bytes = number_of_bytes;
    }

    void
    set_reconnect_interval(int secs)
    {
        connection_manager.set_reconnect_interval(secs);
    }

    void
    add_client(std::string address, std::string port, std::string name = "")
    {
        auto connection_params = ConnectionParams { address, port, transport_protocol };
        if (!connection_params.resolve_address()) {
            throw "Cannot resolve address " + address;
        }
        if (name.empty()) {
            name = connection_params.str();
        }
        auto client = Client { connection_manager, connection_params, name };
        clients.push_back(std::move(client));
        IPX_CTX_INFO(log_ctx, "Added client %s @ %s\n", name.c_str(), connection_params.str().c_str());
    }

    void
    on_session_message(ipx_msg_session_t *session_msg)
    {
        const ipx_session *session = ipx_msg_session_get_session(session_msg);
        switch (ipx_msg_session_get_event(session_msg)) {
            case IPX_MSG_SESSION_OPEN:
                for (auto &client : clients) {
                    open_session(client, session);
                }
                break;
            case IPX_MSG_SESSION_CLOSE:
                for (auto &client : clients) {
                    close_session(client, session);
                }
                break;
        }
    }

    void
    on_ipfix_message(ipx_msg_ipfix_t *ipfix_msg)
    {
        auto message = IPFIXMessage { ipfix_msg };
        switch (forward_mode) {
        case ForwardMode::RoundRobin:
            forward_round_robin(message);
            break;
        case ForwardMode::SendToAll:
            forward_to_all(message);
            break;
        }
    }

    void
    start()
    {
        connection_manager.start();
    }

    void
    stop()
    {
        connection_manager.stop();

        IPX_CTX_INFO(log_ctx, "Total bytes forwarded: %ld", total_bytes);
        IPX_CTX_INFO(log_ctx, "Dropped messages: %ld", dropped_messages);
        IPX_CTX_INFO(log_ctx, "Dropped data records: %ld", dropped_data_records);
    }

    ~Forwarder()
    {
    }

private:
    /// Logging context
    ipx_ctx_t *log_ctx;

    /// Configuration
    TransProto transport_protocol = TransProto::Tcp;
    ForwardMode forward_mode = ForwardMode::SendToAll;
    int template_refresh_interval_secs = 0;
    int template_refresh_interval_bytes = 0;

    /// Mutating state
    ConnectionManager connection_manager;
    std::vector<Client> clients;
    int rr_next_client = 0;

    /// Statistics
    long dropped_messages = 0;
    long dropped_data_records = 0;
    long total_bytes = 0;

    void
    open_session(Client &client, const ipx_session *session_info)
    {
        auto &connection = connection_manager.add_client(client.connection_params);
        auto session_ptr = std::unique_ptr<Session>(new Session { connection, client, session_info->ident });
        IPX_CTX_INFO(log_ctx, "Opened session %s", session_ptr->str().c_str());
        client.sessions[session_info] = std::move(session_ptr);
    }

    void
    close_session(Client &client, const ipx_session *session_info)
    {
        auto &session = *client.sessions[session_info];
        session.connection.close();
        IPX_CTX_INFO(log_ctx, "Closed session %s", session.str().c_str());
        client.sessions.erase(session_info);
    }

    void
    forward_to_all(IPFIXMessage &message)
    {
        for (auto &client : clients) {
            if (!forward_message(client, message)) {
                dropped_messages += 1;
                dropped_data_records += message.drec_count();
            }
        }
    }

    void
    forward_round_robin(IPFIXMessage &message)
    {
        int i = 0;
        while (1) {
            auto &client = next_client();
            if (forward_message(client, message)) {
                break;
            }

            i++;

            // If we went through all the clients multiple times in a row and all the buffers are still full,
            // let's just give up and move onto the next message. If we loop for too long we'll start losing messages!
            if (i < (int)clients.size() * 10) {
                dropped_messages += 1;
                dropped_data_records += message.drec_count();
                break;
            }
        }
    }

    /// Pick the next client in round-robin mode
    Client &
    next_client()
    {
        if (rr_next_client == (int)clients.size()) {
            rr_next_client = 0;
        }
        return clients[rr_next_client++];
    }

    /// Send all templates from the templates snapshot obtained from the message
    /// through the session connection and update the state accordingly
    ///
    /// \return true if there was enough space in the connection buffer, false otherwise
    bool
    send_templates(Session &session, Odid &odid, IPFIXMessage &message)
    {
        auto templates_snapshot = message.get_templates_snapshot();

        auto header = *message.header();
        header.seq_num = htonl(odid.seq_num);

        MessageBuilder builder;
        builder.begin_message(header);

        fds_tsnapshot_for(templates_snapshot,
            [](const fds_template *tmplt, void *data) -> bool {
                auto &builder = *(MessageBuilder *)data;
                builder.write_template(tmplt);
                return true;
            }, &builder);

        builder.finalize_message();

        auto lock = session.connection.begin_write();
        if (builder.message_length() > session.connection.writeable()) {
            // IPX_CTX_WARNING(log_ctx,
            //     "[%s] Cannot send templates because buffer is full! (need %dB, have %ldB)",
            //     odid.str().c_str(), builder.message_length(), session.connection.writeable());
            return false;
        }
        session.connection.write(builder.message_data(), builder.message_length());
        session.connection.commit_write();

        odid.templates_snapshot = templates_snapshot;
        odid.bytes_since_templates_sent = 0;
        odid.last_templates_send_time = std::time(NULL);

        total_bytes += builder.message_length();
        // IPX_CTX_INFO(log_ctx, "[%s] Sent templates", odid.str().c_str());

        return true;
    }

    bool
    should_refresh_templates(Odid &odid)
    {
        if (transport_protocol != TransProto::Udp) {
            return false;
        }
        auto time_since = (std::time(NULL) - odid.last_templates_send_time);
        return (time_since > (unsigned)template_refresh_interval_secs)
            || (odid.bytes_since_templates_sent > (unsigned)template_refresh_interval_bytes);
    }

    bool
    templates_changed(Odid &odid, IPFIXMessage &message)
    {
        auto templates_snapshot = message.get_templates_snapshot();
        return templates_snapshot && odid.templates_snapshot != templates_snapshot;
    }

    /// Forward message to the client, including templates update if needed
    ///
    /// \return true if there was enough space in the connection buffer, false otherwise
    bool
    forward_message(Client &client, IPFIXMessage &message)
    {
        auto &session = *client.sessions[message.session()];

        if (session.connection.connection_lost_flag) {
            for (auto &p : session.odids) {
                p.second.reset_values();
            }
            session.connection.connection_lost_flag = false;
        }

        if (session.odids.find(message.odid()) == session.odids.end()) {
            session.odids[message.odid()] = Odid { session, message.odid() };
            IPX_CTX_INFO(log_ctx, "[%s] Seen new ODID %u", session.str().c_str(), message.odid());
        }
        auto &odid = session.odids[message.odid()];

        if (should_refresh_templates(odid) || templates_changed(odid, message)) {
            if (message.get_templates_snapshot() != nullptr && !send_templates(session, odid, message)) {
                return false;
            }
        }

        auto lock = session.connection.begin_write();
        if (message.length() > session.connection.writeable()) {
            // IPX_CTX_WARNING(log_ctx,
            //     "[%s] Cannot forward message because buffer is full! (need %dB, have %ldB)",
            //     odid.str().c_str(), message.length(), session.connection.writeable());
            return false;
        }

        auto header = *message.header();
        header.seq_num = htonl(odid.seq_num);
        session.connection.write(&header, sizeof(header));
        session.connection.write(message.data() + sizeof(header), message.length() - sizeof(header));
        session.connection.commit_write();

        // IPX_CTX_DEBUG(log_ctx, "[%s] Forwarded message", odid.str().c_str());

        odid.bytes_since_templates_sent += message.length();
        odid.seq_num += message.drec_count();

        total_bytes += message.length();

        return true;
    }
};