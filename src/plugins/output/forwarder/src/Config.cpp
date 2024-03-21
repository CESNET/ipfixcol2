/**
 * \file src/plugins/output/forwarder/src/Config.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Plugin configuration implementation
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

#include "Config.h"

#include <memory>
#include <cctype>
#include <cstring>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

///
/// Config schema definition
///

enum {
    MODE,
    PROTOCOL,
    RECONNECT_SECS,
    TEMPLATES_RESEND_SECS,
    TEMPLATES_RESEND_PKTS,
    CONNECTION_BUFFER_SIZE,
    HOSTS,
    HOST,
    NAME,
    ADDRESS,
    PORT,
    PREMADE_CONNECTIONS
};

static fds_xml_args host_schema[] = {
    FDS_OPTS_ELEM(NAME   , "name"   , FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(ADDRESS, "address", FDS_OPTS_T_STRING, 0             ),
    FDS_OPTS_ELEM(PORT   , "port"   , FDS_OPTS_T_UINT  , 0             ),
    FDS_OPTS_END
};

static fds_xml_args hosts_schema[] = {
    FDS_OPTS_NESTED(HOST, "host", host_schema, FDS_OPTS_P_MULTI),
    FDS_OPTS_END
};

static fds_xml_args params_schema[] = {
    FDS_OPTS_ROOT  ("params"),
    FDS_OPTS_ELEM  (MODE                 , "mode"               , FDS_OPTS_T_STRING, 0             ),
    FDS_OPTS_ELEM  (PROTOCOL             , "protocol"           , FDS_OPTS_T_STRING, 0             ),
    FDS_OPTS_ELEM  (TEMPLATES_RESEND_SECS, "templatesResendSecs", FDS_OPTS_T_UINT  , FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM  (TEMPLATES_RESEND_PKTS, "templatesResendPkts", FDS_OPTS_T_UINT  , FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM  (RECONNECT_SECS       , "reconnectSecs"      , FDS_OPTS_T_UINT  , FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM  (PREMADE_CONNECTIONS  , "premadeConnections" , FDS_OPTS_T_UINT  , FDS_OPTS_P_OPT),
    FDS_OPTS_NESTED(HOSTS                , "hosts"              , hosts_schema     , 0             ),
    FDS_OPTS_END
};

///
/// Config definition
///

/**
 * \brief Create a new configuration
 * \param[in] params XML configuration of JSON plugin
 * \throw runtime_error in case of invalid configuration
 */
Config::Config(const char *xml_config)
{
    set_defaults();

    auto parser = std::unique_ptr<fds_xml_t, decltype(&fds_xml_destroy)>(fds_xml_create(), &fds_xml_destroy);
    if (!parser) {
        throw std::runtime_error("Failed to create an XML parser!");
    }

    if (fds_xml_set_args(parser.get(), params_schema) != FDS_OK) {
        throw std::runtime_error("Failed to parse the description of an XML document!");
    }

    fds_xml_ctx_t *params_ctx = fds_xml_parse_mem(parser.get(), xml_config, true);
    if (!params_ctx) {
        std::string err = fds_xml_last_err(parser.get());
        throw std::runtime_error("Failed to parse the configuration: " + err);
    }

    try {
        parse_params(params_ctx);
        ensure_valid();
    } catch (const std::invalid_argument &ex) {
        throw std::runtime_error("Config params error: " + std::string(ex.what()));
    }
}

void
Config::parse_params(fds_xml_ctx_t *params_ctx)
{
    const fds_xml_cont *content;

    while (fds_xml_next(params_ctx, &content) != FDS_EOC) {

        switch (content->id) {

        case MODE:
            if (strcasecmp(content->ptr_string, "roundrobin") == 0) {
                this->forward_mode = ForwardMode::ROUNDROBIN;

            } else if (strcasecmp(content->ptr_string, "all") == 0) {
                this->forward_mode = ForwardMode::SENDTOALL;

            } else {
                throw std::invalid_argument("mode must be one of: 'RoundRobin', 'All'");

            }
            break;

        case PROTOCOL:
            if (strcasecmp(content->ptr_string, "tcp") == 0) {
                this->protocol = Protocol::TCP;

            } else if (strcasecmp(content->ptr_string, "udp") == 0) {
                this->protocol = Protocol::UDP;

            } else {
                throw std::invalid_argument("protocol must be one of: 'TCP', 'UDP'");

            }
            break;

        case HOSTS:
            parse_hosts(content->ptr_ctx);
            break;

        case TEMPLATES_RESEND_SECS:
            this->tmplts_resend_secs = content->val_uint;
            break;

        case TEMPLATES_RESEND_PKTS:
            this->tmplts_resend_pkts = content->val_uint;
            break;

        case RECONNECT_SECS:
            this->reconnect_secs = content->val_uint;
            break;

        case PREMADE_CONNECTIONS:
            this->nb_premade_connections = content->val_uint;
            break;

        default: assert(0);
        }
    }
}

void
Config::parse_hosts(fds_xml_ctx_t *hosts_ctx)
{
    const fds_xml_cont *content;

    while (fds_xml_next(hosts_ctx, &content) != FDS_EOC) {
        assert(content->id == HOST);
        parse_host(content->ptr_ctx);
    }
}

void
Config::parse_host(fds_xml_ctx_t *host_ctx)
{
    HostConfig host;

    const fds_xml_cont *content;

    while (fds_xml_next(host_ctx, &content) != FDS_EOC) {

        switch (content->id) {

        case NAME:
            host.name = std::string(content->ptr_string);
            break;

        case ADDRESS:
            host.address = std::string(content->ptr_string);
            break;

        case PORT:
            if (content->val_uint > UINT16_MAX) {
                throw std::invalid_argument("invalid host port " + std::to_string(content->val_uint));
            }

            host.port = static_cast<uint16_t>(content->val_uint);
            break;
        }
    }

    if (host.name.empty()) {
        host.name = host.address + ":" + std::to_string(host.port);
    }

    if (host_exists(host)) {
        throw std::invalid_argument("duplicate host " + host.address + ":" + std::to_string(host.port));
    }

    hosts.push_back(host);
}

void
Config::set_defaults()
{
    this->tmplts_resend_secs = 10 * 60;
    this->tmplts_resend_pkts = 5000;
    this->reconnect_secs = 10;
    this->nb_premade_connections = 5;
}

void
Config::ensure_valid()
{
    for (auto &host : hosts) {

        if (!can_resolve_host(host)) {
            throw std::invalid_argument("cannot resolve host address " + host.address);
        }

    }
}

bool
Config::host_exists(HostConfig host)
{
    for (auto &host_ : hosts) {

        if (host.address == host_.address && host.port == host_.port) {
            return true;
        }

    }

    return false;
}

bool
Config::can_resolve_host(HostConfig host)
{
    addrinfo *info;

    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;

    switch (protocol) {
    case Protocol::TCP:
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_socktype = SOCK_STREAM;
        break;

    case Protocol::UDP:
        hints.ai_protocol = IPPROTO_UDP;
        hints.ai_socktype = SOCK_DGRAM;
        break;

    default: assert(0);
    }

    int ret = getaddrinfo(host.address.c_str(), std::to_string(host.port).c_str(), &hints, &info);

    if (ret == 0) {
        freeaddrinfo(info);
        return true;

    } else {
        return false;
    }
}