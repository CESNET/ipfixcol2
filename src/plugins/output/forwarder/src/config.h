/**
 * \file src/plugins/output/forwarder/src/config.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Plugin configuration
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

#include "Forwarder.h"

#include <libfds.h>
#include <ipfixcol2.h>

#include <string>
#include <vector>

/// DIY std::optional
template <typename T>
class Maybe
{
public:
    Maybe()
    : has_value_(false)
    {}

    Maybe(T value)
    : has_value_(true)
    , value_(value)
    {}

    bool
    has_value()
    {
        return has_value_;
    }

    T 
    value()
    {
        return value_;
    }

private:
    bool has_value_;
    T value_;
};


/// Parses the XML config string and configures forwarder accordingly
void 
parse_and_configure(ipx_ctx_t *log_ctx, const char *xml_config, Forwarder &forwarder)
{
    ///
    /// Config schema definition
    ///
    enum {
        MODE, 
        PROTOCOL, 
        RECONNECT_INTERVAL_SECS,
        TEMPLATE_REFRESH_INTERVAL_SECS, 
        TEMPLATE_REFRESH_INTERVAL_BYTES,
        CONNECTION_BUFFER_SIZE,
        HOSTS, 
        HOST, 
        NAME, 
        ADDRESS, 
        PORT
    };

    fds_xml_args host_schema[] = {
        FDS_OPTS_ELEM(NAME   , "name"   , FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
        FDS_OPTS_ELEM(ADDRESS, "address", FDS_OPTS_T_STRING, 0             ),
        FDS_OPTS_ELEM(PORT   , "port"   , FDS_OPTS_T_STRING, 0             ),
        FDS_OPTS_END
    };

    fds_xml_args hosts_schema[] = {
        FDS_OPTS_NESTED(HOST, "host", host_schema, FDS_OPTS_P_MULTI),
        FDS_OPTS_END
    };

    fds_xml_args params_schema[] = {
        FDS_OPTS_ROOT  ("params"),
        FDS_OPTS_ELEM  (MODE                           , "mode"                        , FDS_OPTS_T_STRING, 0             ),
        FDS_OPTS_ELEM  (PROTOCOL                       , "protocol"                    , FDS_OPTS_T_STRING, 0             ),
        FDS_OPTS_ELEM  (CONNECTION_BUFFER_SIZE         , "connectionBufferSize"        , FDS_OPTS_T_INT   , FDS_OPTS_P_OPT),
        FDS_OPTS_ELEM  (TEMPLATE_REFRESH_INTERVAL_SECS , "templateRefreshIntervalSecs" , FDS_OPTS_T_INT   , FDS_OPTS_P_OPT),
        FDS_OPTS_ELEM  (TEMPLATE_REFRESH_INTERVAL_BYTES, "templateRefreshIntervalBytes", FDS_OPTS_T_INT   , FDS_OPTS_P_OPT),
        FDS_OPTS_ELEM  (RECONNECT_INTERVAL_SECS        , "reconnectIntervalSecs"       , FDS_OPTS_T_INT   , FDS_OPTS_P_OPT),
        FDS_OPTS_NESTED(HOSTS                          , "hosts"                       , hosts_schema     , 0             ),
        FDS_OPTS_END
    };

    ///
    /// Default parameter values
    ///
    const int default_template_refresh_interval_secs  = 10 * 60;
    const int default_template_refresh_interval_bytes = 5 * 1024 * 1024;
    const int default_reconnect_interval_secs         = 10;
    const int default_connection_buffer_size          = 4 * 1024 * 1024;
    
    ///
    /// Parsed parameters
    ///
    struct HostInfo {
        std::string name;
        std::string address;
        std::string port;
    };

    std::string mode;
    std::string protocol;
    std::vector<HostInfo> hosts;
    Maybe<int> connection_buffer_size;
    Maybe<int> template_refresh_interval_secs;
    Maybe<int> template_refresh_interval_bytes;
    Maybe<int> reconnect_interval_secs;
    
    ///
    /// Parsing
    ///
    auto parser = std::unique_ptr<fds_xml_t, decltype(&fds_xml_destroy)>(fds_xml_create(), &fds_xml_destroy);
    if (!parser) {
        throw std::string("Cannot create XML parser");
    }

    auto lower = [](std::string s) {
        for (auto &c : s) {
            c = std::tolower(c);
        }
        return s;
    };

    auto parser_error = [&]() {
        throw std::string("XML parser error: ") + fds_xml_last_err(parser.get());
    };

    if (fds_xml_set_args(parser.get(), params_schema) != FDS_OK) {
        parser_error();
    }

    auto params_elem = fds_xml_parse_mem(parser.get(), xml_config, true);
    if (!params_elem) {
        parser_error();
    }

    auto process_host = [&](fds_xml_ctx_t *host_elem) {
        HostInfo host;
        const fds_xml_cont *content;
        while (fds_xml_next(host_elem, &content) != FDS_EOC) {
            switch (content->id) {
            case NAME:
                host.name = std::string(content->ptr_string);
                break;
            case ADDRESS:
                host.address = std::string(content->ptr_string);
                break;
            case PORT:
                host.port = std::string(content->ptr_string);
                break;
            }
        }
        hosts.push_back(host);
    };

    auto process_hosts = [&](fds_xml_ctx_t *hosts_elem) {
        const fds_xml_cont *content;
        while (fds_xml_next(hosts_elem, &content) != FDS_EOC) {
            process_host(content->ptr_ctx);
        }
    };

    const fds_xml_cont *content;
    while (fds_xml_next(params_elem, &content) != FDS_EOC) {
        switch (content->id) {
        case MODE:
            mode = std::string(content->ptr_string);
            break;
        case PROTOCOL:
            protocol = std::string(content->ptr_string);
            break;
        case HOSTS:
            process_hosts(content->ptr_ctx);
            break;
        case CONNECTION_BUFFER_SIZE:
            connection_buffer_size = content->val_int;
            break;
        case TEMPLATE_REFRESH_INTERVAL_SECS:
            template_refresh_interval_secs = content->val_int;
            break;
        case TEMPLATE_REFRESH_INTERVAL_BYTES:
            template_refresh_interval_bytes = content->val_int;
            break;
        case RECONNECT_INTERVAL_SECS:
            reconnect_interval_secs = content->val_int;
            break;
        }
    }
    
    ///
    /// Check parameters and configure
    ///
    if (lower(mode) == "all" || lower(mode) == "send to all" || lower(mode) == "send-to-all") {
        forwarder.set_forward_mode(ForwardMode::SendToAll);
    } else if (lower(mode) == "roundrobin" || lower(mode) == "round robin" || lower(mode) == "round-robin") {
        forwarder.set_forward_mode(ForwardMode::RoundRobin);
    } else {
        throw "Invalid mode '" + mode + "', possible values are: 'roundrobin', 'all'";
    }

    if (lower(protocol) == "udp") {
        forwarder.set_transport_protocol(TransProto::Udp);
    } else if (lower(protocol) == "tcp") {
        forwarder.set_transport_protocol(TransProto::Tcp);
    } else {
        throw "Invalid protocol '" + protocol + "', possible values are: 'tcp', 'udp'";
    }

    if (connection_buffer_size.has_value()) {
        if (connection_buffer_size.value() > 0) {
            forwarder.set_connection_buffer_size(connection_buffer_size.value());
        } else {
            throw std::string("Invalid connection buffer size");
        }
    } else {
        forwarder.set_connection_buffer_size(default_connection_buffer_size);
    }

    if (template_refresh_interval_secs.has_value()) {
        if (template_refresh_interval_secs.value() >= 0) {
            if (lower(protocol) == "tcp") {
                IPX_CTX_WARNING(log_ctx, "Templates refresh interval is set but transport protocol is TCP");
            }
            forwarder.set_template_refresh_interval_secs(template_refresh_interval_secs.value());
        } else {
            throw std::string("Invalid template refresh secs interval");
        }
    } else {
        forwarder.set_template_refresh_interval_secs(default_template_refresh_interval_secs);
    }

    if (template_refresh_interval_bytes.has_value()) {
        if (template_refresh_interval_bytes.value() >= 0) {
            if (lower(protocol) == "tcp") {
                IPX_CTX_WARNING(log_ctx, "Templates refresh interval is set but transport protocol is TCP");
            }
            forwarder.set_template_refresh_interval_bytes(template_refresh_interval_bytes.value());
        } else {
            throw std::string("Invalid template refresh bytes interval");
        }
    } else {
        forwarder.set_template_refresh_interval_secs(default_template_refresh_interval_bytes);
    }

    if (template_refresh_interval_bytes.has_value()) {
        if (reconnect_interval_secs.value() >= 0) {
            if (lower(protocol) == "udp") {
                IPX_CTX_WARNING(log_ctx, "Reconnect interval is set but transport protocol is UDP");
            }
            forwarder.set_reconnect_interval(reconnect_interval_secs.value());
        } else {
            throw std::string("Invalid reconnect interval");
        }
    } else {
        forwarder.set_template_refresh_interval_secs(default_reconnect_interval_secs);
    }

    for (auto &host : hosts) {
        forwarder.add_client(host.address, host.port, host.name);
    }
}