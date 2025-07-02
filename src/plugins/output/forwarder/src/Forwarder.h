/**
 * \file src/plugins/output/forwarder/src/Forwarder.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Forwarder class header
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

#include "Config.h"

#include <vector>
#include <memory>

#include "Host.h"
#include "common.h"
#include "connector/Connector.h"

/// A class representing the forwarder itself
class Forwarder {
public:
    /**
     * \brief The constructor
     * \param config   The forwarder configuration
     * \param log_ctx  The logging context
     */
    Forwarder(Config config, ipx_ctx_t *log_ctx);

    /**
     * Disable copy and move constructors
     */
    Forwarder(const Forwarder &) = delete;
    Forwarder(Forwarder &&) = delete;

    /**
     * \brief Handle a session message
     * \param msg  The session message
     */
    void
    handle_session_message(ipx_msg_session_t *msg);

    /**
     * \brief Handle an IPFIX message
     * \param msg  The IPFIX message
     */
    void
    handle_ipfix_message(ipx_msg_ipfix_t *msg);

    /**
     * \brief Handle a periodic message
     * \param msg  The periodic message
     */
    void
    handle_periodic_message(ipx_msg_periodic_t *msg);

    /**
     * \brief The destructor - finalize the forwarder
     */
    ~Forwarder() {}

private:
    Config m_config;

    ipx_ctx_t *m_log_ctx;

    std::vector<std::unique_ptr<Host>> m_hosts;

    size_t m_rr_index = 0;

    std::unique_ptr<Connector> m_connector;

    void
    forward_to_all(ipx_msg_ipfix_t *msg);

    void
    forward_round_robin(ipx_msg_ipfix_t *msg);

};
