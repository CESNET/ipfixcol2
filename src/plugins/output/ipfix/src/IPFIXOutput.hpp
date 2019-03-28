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
#include <vector>
#include <cstdio>

#include <ipfixcol2.h>
#include <libfds.h>

class IPFIXOutput {
private:
    const ipx_ctx *plugin_context;
    Config *config;
    
    struct odid_context_s {
        uint32_t odid;
        const ipx_session *session = NULL;
        std::set<uint16_t> templates_seen;
        std::set<uint16_t> options_templates_seen;
        std::set<const ipx_session *> colliding_sessions;
        bool needs_to_write_templates = false;
        uint32_t sequence_number = 0;
    };

    std::map<uint32_t, odid_context_s> odid_contexts;
    std::FILE *output_file = NULL;
    uint32_t file_start_export_time;

    bool
    should_start_new_file(uint32_t export_time);
    void 
    new_file(uint32_t export_time);
    void
    write_bytes(const void *bytes, size_t bytes_count);
    void
    close_file();

    void
    remove_dead_templates(odid_context_s *odid_context, const fds_tsnapshot_t *templates_snapshot);
    int
    write_template_set(uint16_t set_id, const fds_tsnapshot_t *templates_snapshot, 
                       std::set<uint16_t>::iterator template_ids, std::set<uint16_t>::iterator template_ids_end, 
                       int size_limit, int *out_set_length);
    void
    write_templates(odid_context_s *odid_context, const fds_tsnapshot_t *templates_snapshot,
                    uint32_t export_time, uint32_t sequence_number);


public:
    IPFIXOutput(Config *config, const ipx_ctx *ctx);
    ~IPFIXOutput();

    /**
     * \brief      Processes an incoming IPFIX message from the collector
     *
     * \param      message  The IPFIX message
     */
    void 
    on_ipfix_message(ipx_msg_ipfix *message);

    /**
     * \brief      Processes an incoming session message from the collector
     *
     * \param      message  The session message
     */
    void 
    on_session_message(ipx_msg_session *message);
};


#endif // IPFIXOUTPUT_HPP
