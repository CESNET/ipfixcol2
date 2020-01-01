/**
 * \file src/plugins/output/ipfix/src/IPFIXOutput.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief IPFIX output plugin logic (header file)
 * \date 2019
 */

/* Copyright (C) 2019 CESNET, z.s.p.o.
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
#ifndef IPFIXOUTPUT_HPP
#define IPFIXOUTPUT_HPP

#include "Config.hpp"

#include <set>
#include <map>
#include <memory>
#include <vector>
#include <cstdio>
#include <ctime>

#include <ipfixcol2.h>
#include <libfds.h>

/// IPFIX file manager
class IPFIXOutput {
private:
    /// Plugin context (only for log!)
    const ipx_ctx *plugin_context;
    /// Parsed instance configuration
    const Config *config;

    /// Auxiliary context of an Observation Domain ID (ODID)
    struct odid_context_s {
        /// Transport Session with permission to write to the file
        const ipx_session *session = nullptr;
        /// Detected Transport Sessions without permission to write to the file
        std::set<const ipx_session *> colliding_sessions;
        /// All (Options) Templates must be written before any Data Records
        bool needs_to_write_templates = false;
        /// Sequence number of the IPFIX Message (only if skipping unknown Data Sets is enabled)
        uint32_t sequence_number = 0;
    };

    /// Memory for editing IPFIX Messages
    std::unique_ptr<uint8_t[]> buffer = nullptr;
    /// Map of known Observation Domain IDs (ODIDs)
    std::map<uint32_t, odid_context_s> odid_contexts;
    /// Current output file
    std::FILE *output_file = nullptr;
    /// Start time of the current file
    std::time_t file_start_time = 0;

    struct odid_context_s *
    get_odid(uint32_t odid, const ipx_session *session);
    void
    remove_session(const struct ipx_session *session);
    bool
    should_start_new_file(std::time_t current_time);
    void
    new_file(const std::time_t current_time);
    void
    close_file();
    void
    write_templates(const fds_tsnapshot_t *snap, uint32_t odid, uint32_t exp_time, uint32_t seq_num);

public:
    /**
     * \brief Constructor
     * \param[in] config Instance configuration
     * \param[in] ctx    Plugin context (for log only!)
     */
    IPFIXOutput(const Config *config, const ipx_ctx *ctx);
    /// Class destructor
    ~IPFIXOutput();

    /**
     * \brief  Processes an incoming IPFIX message from the collector
     * \param[in] message The IPFIX message
     * \throws runtime_error if a new file cannot be created
     *
     */
    void
    on_ipfix_message(ipx_msg_ipfix *message);

    /**
     * \brief Processes an incoming session message from the collector
     * \param[in] message The session message
     */
    void
    on_session_message(ipx_msg_session *message);
};


#endif // IPFIXOUTPUT_HPP
