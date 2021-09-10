/**
 * \file src/plugins/output/forwarder/src/Message.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Message class implementation
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

#include "Message.h"
#include <cassert>

static uint16_t
get_set_id(fds_template_type template_type)
{
    switch (template_type) {
    case FDS_TYPE_TEMPLATE:
        return FDS_IPFIX_SET_TMPLT;

    case FDS_TYPE_TEMPLATE_OPTS:
        return FDS_IPFIX_SET_OPTS_TMPLT;

    default: assert(0);
    }
}

void
Message::add_set(const fds_ipfix_set_hdr *set)
{
    if (m_current_set_hdr) {
        finalize_set();
    }

    uint16_t set_len = ntohs(set->length);
    add_part((uint8_t *) set, set_len);
    m_current_set_hdr = nullptr;
}

void
Message::add_template(const fds_template *tmplt)
{
    require_set(get_set_id(tmplt->type));
    write(tmplt->raw.data, tmplt->raw.length);
    m_current_set_hdr->length += tmplt->raw.length;
}

void
Message::add_template_withdrawal_all()
{
    finalize_set();

    require_set(FDS_IPFIX_SET_TMPLT);
    uint16_t wdrl2[2] = { htons(FDS_IPFIX_SET_TMPLT), 0 };
    write(&wdrl2);
    m_current_set_hdr->length += sizeof(wdrl2);
    finalize_set();

    require_set(FDS_IPFIX_SET_OPTS_TMPLT);
    uint16_t wdrl3[2] = { htons(FDS_IPFIX_SET_OPTS_TMPLT), 0 };
    write(&wdrl3);
    m_current_set_hdr->length += sizeof(wdrl3);
    finalize_set();
}

void
Message::finalize()
{
    finalize_set();
    m_msg_hdr->length = htons(m_length);
}

void
Message::start(const fds_ipfix_msg_hdr *msg_hdr)
{
    m_parts.clear();
    m_length = 0;
    m_buffer_pos = 0;
    m_last_part_from_buffer = false;
    m_msg_hdr = nullptr;
    m_current_set_hdr = nullptr;

    m_msg_hdr = write(msg_hdr);
}

void
Message::add_part(uint8_t *data, uint16_t length)
{
    assert(data < m_buffer || data >= m_buffer + BUFFER_SIZE);

    iovec part;
    part.iov_base = data;
    part.iov_len = length;
    m_parts.push_back(part);

    m_length += length;

    m_last_part_from_buffer = false;
}

uint8_t *
Message::write(const uint8_t *data, uint16_t length)
{
    assert(BUFFER_SIZE - m_buffer_pos >= length);

    uint8_t *p = &m_buffer[m_buffer_pos];
    m_buffer_pos += length;
    memcpy(p, data, length);

    // If the last part is also allocated from our buffer, we can just expand it rather than pushing a new part
    if (m_last_part_from_buffer) {
        m_parts.back().iov_len += length;

    } else {
        iovec part;
        part.iov_base = p;
        part.iov_len = length;
        m_parts.push_back(part);

        m_last_part_from_buffer = true;
    }

    m_length += length;

    return p;
}

void
Message::require_set(uint16_t set_id)
{
    if (!m_current_set_hdr || m_current_set_hdr->flowset_id != set_id) {
        finalize_set();

        fds_ipfix_set_hdr hdr;
        hdr.flowset_id = set_id;
        hdr.length = sizeof(fds_ipfix_set_hdr);
        m_current_set_hdr = write(&hdr);
    }
}

void
Message::finalize_set()
{
    // If there is no set, do nothing
    if (!m_current_set_hdr) {
        return;
    }

    assert(m_current_set_hdr->length > sizeof(fds_ipfix_set_hdr));

    m_current_set_hdr->flowset_id = htons(m_current_set_hdr->flowset_id);
    m_current_set_hdr->length = htons(m_current_set_hdr->length);
    m_current_set_hdr = nullptr;
}