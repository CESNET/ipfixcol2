/**
 * \file src/plugins/output/json/src/Storage.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief JSON converter and output manager (source file)
 * \date 2018-2019
 */

/* Copyright (C) 2018-2019 CESNET, z.s.p.o.
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

#include <stdexcept>
#include <cstring>
#include <inttypes.h>

using namespace std;
#include "Storage.hpp"
#include <libfds.h>

/** Base size of the conversion buffer           */
#define BUFFER_BASE   4096

Storage::Storage(const ipx_ctx_t *ctx, const struct cfg_format &fmt)
    : m_ctx(ctx), m_format(fmt)
{
    // Prepare the buffer
    m_record.buffer = nullptr;
    m_record.size_used = 0;
    m_record.size_alloc = 0;

    // Prepare conversion flags
    m_flags = FDS_CD2J_ALLOW_REALLOC; // Allow automatic reallocation of the buffer
    if (m_format.tcp_flags) {
        m_flags |= FDS_CD2J_FORMAT_TCPFLAGS;
    }
    if (m_format.timestamp) {
        m_flags |= FDS_CD2J_TS_FORMAT_MSEC;
    }
    if (m_format.proto) {
        m_flags |= FDS_CD2J_FORMAT_PROTO;
    }
    if (m_format.ignore_unknown) {
        m_flags |= FDS_CD2J_IGNORE_UNKNOWN;
    }
    if (!m_format.white_spaces) {
        m_flags |= FDS_CD2J_NON_PRINTABLE;
    }
    if (m_format.numeric_names) {
        m_flags |= FDS_CD2J_NUMERIC_ID;
    }
    if (m_format.split_biflow) {
        m_flags |= FDS_CD2J_REVERSE_SKIP;
    }

}

Storage::~Storage()
{
    // Destroy all outputs
    for (Output *output : m_outputs) {
        delete output;
    }

    free(m_record.buffer);
}

/**
 * \brief Reserve memory of the conversion buffer
 *
 * Requests that the string capacity be adapted to a planned change in size to a length of up
 * to n characters.
 * \param[in] n Minimal size of the buffer
 * \throws bad_alloc in case of a memory allocation error
 */
void
Storage::buffer_reserve(size_t n)
{
    if (n <= buffer_alloc()) {
        // Nothing to do
        return;
    }

    // Prepare a new buffer and copy the content
    const size_t new_size = ((n / BUFFER_BASE) + 1) * BUFFER_BASE;
    char *new_buffer = (char *) realloc(m_record.buffer, new_size * sizeof(char));
    if (!new_buffer) {
        throw std::bad_alloc();
    }

    m_record.buffer = new_buffer;
    m_record.size_alloc = new_size;
}

/**
 * \brief Append the conversion buffer
 * \note
 *   If the buffer length is not sufficient enough, it is automatically reallocated to fit
 *   the string.
 * \param[in] str String to add
 * \throws bad_alloc in case of a memory allocation error
 */
void
Storage::buffer_append(const char *str)
{
    const size_t len = std::strlen(str) + 1; // "\0"
    buffer_reserve(buffer_used() + len);
    memcpy(m_record.buffer + buffer_used(), str, len);
    m_record.size_used += len - 1;
}

void
Storage::output_add(Output *output)
{
    m_outputs.push_back(output);
}

int
Storage::records_store(ipx_msg_ipfix_t *msg, const fds_iemgr_t *iemgr)
{
    // Process all data records
    const uint32_t rec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);

    // Message header
    auto hdr = (fds_ipfix_msg_hdr*) ipx_msg_ipfix_get_packet(msg);

    for (uint32_t i = 0; i < rec_cnt; ++i) {
        ipx_ipfix_record *ipfix_rec = ipx_msg_ipfix_get_drec(msg, i);

        // Convert the record
        if (m_format.ignore_options && ipfix_rec->rec.tmplt->type == FDS_TYPE_TEMPLATE_OPTS) {
            // Skip records based on Options Template
            continue;
        }

        // Convert the record
        convert(ipfix_rec->rec, iemgr, hdr, false);

        // Store it
        for (Output *output : m_outputs) {
            if (output->process(m_record.buffer, m_record.size_used) != IPX_OK) {
                return IPX_ERR_DENIED;
            }
        }

        if (!m_format.split_biflow || (ipfix_rec->rec.tmplt->flags & FDS_TEMPLATE_BIFLOW) == 0) {
            // Record splitting is disabled or it is not a biflow record -> continue
            continue;
        }

        // Convert the record from reverse point of view
        convert(ipfix_rec->rec, iemgr, hdr, true);

        // Store it
        for (Output *output : m_outputs) {
            if (output->process(m_record.buffer, m_record.size_used) != IPX_OK) {
                return IPX_ERR_DENIED;
            }
        }
    }

    return IPX_OK;
}

/**
 * \brief Convert an IPFIX record to JSON string
 *
 * For each field in the record, try to convert it into JSON format. The record is stored
 * into the local buffer.
 * \param[in] rec     IPFIX record to convert
 * \param[in] iemgr   Manager of Information Elements
 * \param[in] reverse Convert from reverse point of view (affects only biflow records)
 * \throw runtime_error if the JSON converter fails
 */
void
Storage::convert(struct fds_drec &rec, const fds_iemgr_t *iemgr, fds_ipfix_msg_hdr *hdr, bool reverse)
{
    // Convert the record
    uint32_t flags = m_flags;
    flags |= reverse ? FDS_CD2J_BIFLOW_REVERSE : 0;

    int rc = fds_drec2json(&rec, flags, iemgr, &m_record.buffer, &m_record.size_alloc);
    if (rc < 0) {
        throw std::runtime_error("Conversion to JSON failed (probably a memory allocation error)!");
    }

    m_record.size_used = size_t(rc);

    if (m_format.detailed_info) {

        // Remove '}' parenthesis at the end of the record
        m_record.size_used--;

        // Array for formatting detailed info fields
        char field[64];
        snprintf(field, 64, ",\"ipfix:exportTime\":\"%" PRIu32 "\"", ntohl(hdr->export_time));
        buffer_append(field);

        snprintf(field, 64, ",\"ipfix:seqNumber\":\"%" PRIu32 "\"", ntohl(hdr->seq_num));
        buffer_append(field);

        snprintf(field, 64, ",\"ipfix:odid\":\"%" PRIu32 "\"", ntohl(hdr->odid));
        buffer_append(field);

        snprintf(field, 32, ",\"ipfix:msgLength\":\"%" PRIu16 "\"", ntohs(hdr->length));
        buffer_append(field);

        snprintf(field, 32, ",\"ipfix:templateId\":\"%" PRIu16 "\"", rec.tmplt->id);
        buffer_append(field);

        // Append the record with '}' parenthesis removed before
        buffer_append("}");
    }

         // Append the record with end of line character
         buffer_append("\n");

    /* Note: additional information (e.g. ODID, Export Time, etc.) can be added here,
     * just use buffer_append() and buffer_reserve() to append and extend buffer, respectively.
     */
}