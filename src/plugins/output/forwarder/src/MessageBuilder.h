/**
 * \file src/plugins/output/forwarder/src/MessageBuilder.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief IPFIX message builder
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
#include <arpa/inet.h>

#include <vector>
#include <cstring>

class MessageBuilder
{
public:
    void
    begin_message(fds_ipfix_msg_hdr message_header)
    {
        write(&message_header, sizeof(message_header));
    }

    void
    write_template(const fds_template *tmplt)
    {
        if (current_set_id != get_template_set_id(tmplt->type)) {
            end_template_set();
            begin_template_set(tmplt->type);
        }

        write(tmplt->raw.data, tmplt->raw.length);
        current_set_length += tmplt->raw.length;
    }

    void 
    finalize_message()
    {
        end_template_set();
        header()->length = htons(write_offset);
    }

    uint8_t *
    message_data()
    {
        return &buffer[0];
    }

    int 
    message_length()
    {
        return write_offset;
    }

private:
    std::vector<uint8_t> buffer;
    int write_offset = 0;
    int set_header_offset = -1;
    int current_set_id = 0;
    int current_set_length = 0;

    fds_ipfix_msg_hdr *
    header()
    {
        return (fds_ipfix_msg_hdr *)&buffer[0];
    }

    fds_ipfix_set_hdr *
    current_set_header()
    {
        return (fds_ipfix_set_hdr *)&buffer[set_header_offset];
    }

    void 
    write(void *data, int length)
    {
        if (((int)buffer.size() - write_offset) < length) {
            buffer.resize(buffer.size() + 1024);
            return write(data, length);
        }
        std::memcpy(&buffer[write_offset], data, length);
        write_offset += length;
    }

    uint16_t 
    get_template_set_id(fds_template_type template_type)
    {
        return (template_type == FDS_TYPE_TEMPLATE
            ? FDS_IPFIX_SET_TMPLT : FDS_IPFIX_SET_OPTS_TMPLT);
    }

    void
    begin_template_set(fds_template_type template_type)
    {   
        fds_ipfix_set_hdr set_header = {};
        set_header.flowset_id = htons(get_template_set_id(template_type));
        
        set_header_offset = write_offset;
        write(&set_header, sizeof(set_header));

        current_set_length = sizeof(set_header);
        current_set_id = get_template_set_id(template_type);
    }

    void
    end_template_set()
    {
        if (set_header_offset != -1) {
            current_set_header()->length = htons(current_set_length);
        }
        current_set_id = 0;
        current_set_length = 0;
        set_header_offset = -1;
    }
};