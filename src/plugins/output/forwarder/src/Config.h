/**
 * \file src/plugins/output/forwarder/src/Config.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Plugin configuration header
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

#include <libfds.h>

#include <string>
#include <vector>

#include "common.h"

/// The forwarding mode
enum class ForwardMode {
    UNASSIGNED,
    SENDTOALL, /// Every message is forwarded to all of the hosts
    ROUNDROBIN /// Only one host receives each message, next host is selected every message
};

struct HostConfig {
    /// The displayed name of the host, purely informational
    std::string name;
    /// The address of the host, IP address or a hostname
    std::string address;
    /// The port of the host
    uint16_t port;
};

/// The config to be passed to the forwarder
class Config
{
public:
    /// The transport protocol to be used for connection to the hosts
    Protocol protocol;
    /// The mode of forwarding messages
    ForwardMode forward_mode;
    /// Connection parameters of the hosts the data will be forwarded to
    std::vector<HostConfig> hosts;
    /// The number of packets sent between sending refresh of templates (whichever happens first, packets or secs)
    unsigned int tmplts_resend_pkts;
    /// The number of seconds elapsed between sending refresh of templates (whichever happens first, packets or secs)
    unsigned int tmplts_resend_secs;
    /// The number of seconds to wait before trying to reconnect when using a TCP connection
    unsigned int reconnect_secs;
    /// Number of premade connections to keep
    unsigned int nb_premade_connections;

    Config() {};

    /**
     * \brief Create a new configuration
     * \param[in] params XML configuration of the plugin
     * \throw runtime_error in case of invalid configuration
     */
    Config(const char *xml_config);

private:
    /// Parse the <params> element
    void
    parse_params(fds_xml_ctx_t *params_ctx);

    /// Parse the <hosts> element
    void
    parse_hosts(fds_xml_ctx_t *hosts_ctx);

    /// Parse the <host> element
    void
    parse_host(fds_xml_ctx_t *host_ctx);

    /// Set default config values
    void
    set_defaults();

    /// Ensure the configuration is valid or throw an exception
    void
    ensure_valid();

    /// Check if a host already exists in the vector of hosts
    bool
    host_exists(HostConfig host);

    /// Check if the host address can be resolved
    bool
    can_resolve_host(HostConfig host);
};