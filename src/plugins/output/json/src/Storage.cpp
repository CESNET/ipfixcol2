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

/** Base size of the conversion buffer                 */
#define BUFFER_BASE   4096
/** Size of local conversion buffers (for snprintf)    */
#define LOCAL_BSIZE   64

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
    if (!m_format.octets_as_uint) {
        m_flags |= FDS_CD2J_OCTETS_NOINT;
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

/**
 * \brief Get IP address from Transport Session
 *
 * \note Not all Transport Session contains an IPv4/IPv6 address (for example, file type)
 * \param[in]  ipx_desc Transport Session description
 * \param[out] src_addr Conversion buffer for IPv4/IPv6 address converted to string
 * \param[int] size     Size of the conversion buffer
 * \return On success returns a pointer to the buffer. Otherwise returns nullptr.
 */
const char *
Storage::session_src_addr(const struct ipx_session *ipx_desc, char *src_addr, socklen_t size)
{
    const struct ipx_session_net *net_desc;
    switch (ipx_desc->type) {
    case FDS_SESSION_UDP:
        net_desc = &ipx_desc->udp.net;
        break;
    case FDS_SESSION_TCP:
        net_desc = &ipx_desc->tcp.net;
        break;
    case FDS_SESSION_SCTP:
        net_desc = &ipx_desc->sctp.net;
        break;
    default:
        return nullptr;
    }

    const char *ret;
    if (net_desc->l3_proto == AF_INET) {
        ret = inet_ntop(AF_INET, &net_desc->addr_src.ipv4, src_addr, size);
    } else {
        ret = inet_ntop(AF_INET6, &net_desc->addr_src.ipv6, src_addr, size);
    }

    return ret;
}

/**
 * \brief Convert template record to JSON string
 *
 * \param[in] tset_iter  (Options) Template Set structure to convert
 * \param[in] set_id     Id of the Template Set
 * \param[in] hdr        Message header of IPFIX record
 * \throw runtime_error  If template parser failed
 */
void
Storage::convert_tmplt_rec(struct fds_tset_iter *tset_iter, uint16_t set_id, const struct fds_ipfix_msg_hdr *hdr)
{
    enum fds_template_type type;
    void *ptr;
    if (set_id == FDS_IPFIX_SET_TMPLT) {
        buffer_append("{\"@type\":\"ipfix.template\",");
        type = FDS_TYPE_TEMPLATE;
        ptr = tset_iter->ptr.trec;
    } else {
        assert(set_id == FDS_IPFIX_SET_OPTS_TMPLT);
        buffer_append("{\"@type\":\"ipfix.optionsTemplate\",");
        type = FDS_TYPE_TEMPLATE_OPTS;
        ptr = tset_iter->ptr.opts_trec;
    }

    // Filling the template structure with data from raw packet
    uint16_t tmplt_size = tset_iter->size;
    struct fds_template *tmplt;
    int rc;
    rc = fds_template_parse(type, ptr, &tmplt_size, &tmplt);
    if (rc != FDS_OK) {
        throw std::runtime_error("Parsing failed due to memory allocation error or the format of template is invalid!");
    }

    // Printing out the header
    char field[LOCAL_BSIZE];
    snprintf(field, LOCAL_BSIZE, "\"ipfix:templateId\":%" PRIu16, tmplt->id);
    buffer_append(field);
    if (set_id == FDS_IPFIX_SET_OPTS_TMPLT) {
        snprintf(field, LOCAL_BSIZE, ",\"ipfix:scopeCount\":%" PRIu16, tmplt->fields_cnt_scope);
        buffer_append(field);
    }

    // Add detailed info to record
    if (m_format.detailed_info) {
        addDetailedInfo(hdr);
    }

    buffer_append(",\"ipfix:fields\":[");

    // Iteration through the fields and converting them to JSON string
    for (uint16_t i = 0; i < tmplt->fields_cnt_total; i++) {
        struct fds_tfield current = tmplt->fields[i];
        if (i != 0) { // Not first field
            buffer_append(",");
        }
        buffer_append("{");
        snprintf(field, LOCAL_BSIZE, "\"ipfix:elementId\":%" PRIu16, current.id);
        buffer_append(field);
        snprintf(field, LOCAL_BSIZE, ",\"ipfix:enterpriseId\":%" PRIu32, current.en);
        buffer_append(field);
        snprintf(field, LOCAL_BSIZE, ",\"ipfix:fieldLength\":%" PRIu16, current.length);
        buffer_append(field);
        buffer_append("}");
    }
    buffer_append("]}\n");

    // Free allocated memory
    fds_template_destroy(tmplt);
}

/**
 * \brief Convert Template sets and Options Template sets
 *
 * From all sets in the Message, try to convert just Template and Options template sets.
 * \param[in] set   All sets in the Message
 * \param[in] hdr   Message header of IPFIX record
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED if an output fails to store any record
 */
int
Storage::convert_tset(struct ipx_ipfix_set *set, const struct fds_ipfix_msg_hdr *hdr)
{
    uint16_t set_id = ntohs(set->ptr->flowset_id);
    assert(set_id == FDS_IPFIX_SET_TMPLT || set_id == FDS_IPFIX_SET_OPTS_TMPLT);

    // Template set
    struct fds_tset_iter tset_iter;
    fds_tset_iter_init(&tset_iter, set->ptr);

    // Iteration through all (Options) Templates in the Set
    while (fds_tset_iter_next(&tset_iter) == FDS_OK) {
        // Read and print single template
        convert_tmplt_rec(&tset_iter, set_id, hdr);

        // Store it
        for (Output *output : m_outputs) {
            if (output->process(m_record.buffer, m_record.size_used) != IPX_OK) {
                return IPX_ERR_DENIED;
            }
        }

        // Buffer is empty
        m_record.size_used = 0;
    }

    return IPX_OK;
}

int
Storage::records_store(ipx_msg_ipfix_t *msg, const fds_iemgr_t *iemgr)
{
    const auto hdr = (fds_ipfix_msg_hdr*) ipx_msg_ipfix_get_packet(msg);
    const uint32_t rec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);
    bool flush = false;
    int ret = IPX_OK;

    // Extract IPv4/IPv6 address of the exporter, if required
    m_src_addr = nullptr;
    char src_addr[INET6_ADDRSTRLEN];
    if (m_format.detailed_info) {
        const struct ipx_msg_ctx *msg_ctx = ipx_msg_ipfix_get_ctx(msg);
        m_src_addr = session_src_addr(msg_ctx->session, src_addr, INET6_ADDRSTRLEN);
    }

    // Process (Options) Template records if enabled
    if (m_format.template_info) {
        struct ipx_ipfix_set *sets;
        size_t set_cnt;
        ipx_msg_ipfix_get_sets(msg, &sets, &set_cnt);

        // Iteration through all sets
        for (uint32_t i = 0; i < set_cnt; i++) {
            uint16_t set_id = ntohs(sets[i].ptr->flowset_id);
            if (set_id != FDS_IPFIX_SET_TMPLT && set_id != FDS_IPFIX_SET_OPTS_TMPLT) {
                // Skip non-template sets
                continue;
            }

            flush = true;
            if (convert_tset(&sets[i], hdr) != IPX_OK) {
                ret = IPX_ERR_DENIED;
                goto endloop;
            }
        }
    }

    // Process all data records
    for (uint32_t i = 0; i < rec_cnt; ++i) {
        ipx_ipfix_record *ipfix_rec = ipx_msg_ipfix_get_drec(msg, i);

        // Convert the record
        if (m_format.ignore_options && ipfix_rec->rec.tmplt->type == FDS_TYPE_TEMPLATE_OPTS) {
            // Skip records based on Options Template
            continue;
        }

        flush = true;

        // Convert the record
        convert(ipfix_rec->rec, iemgr, hdr, false);

        // Store it
        for (Output *output : m_outputs) {
            if (output->process(m_record.buffer, m_record.size_used) != IPX_OK) {
                ret = IPX_ERR_DENIED;
                goto endloop;
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
                ret = IPX_ERR_DENIED;
                goto endloop;
            }
        }
    }

endloop:
    if (flush) {
        for (Output *output : m_outputs) {
            output->flush();
        }
    }

    return ret;
}

/**
 * \brief Add fields with detailed info (export time, sequence number, ODID, message length) to record
 *
 * For each record, add detailed information if detailedInfo is enabled.
 * @param[in] hdr   Message header of IPFIX record
 */
void
Storage::addDetailedInfo(const struct fds_ipfix_msg_hdr *hdr)
{
    // Array for formatting detailed info fields
    char field[LOCAL_BSIZE];
    snprintf(field, LOCAL_BSIZE, ",\"ipfix:exportTime\":%" PRIu32, ntohl(hdr->export_time));
    buffer_append(field);

    snprintf(field, LOCAL_BSIZE, ",\"ipfix:seqNumber\":%" PRIu32, ntohl(hdr->seq_num));
    buffer_append(field);

    snprintf(field, LOCAL_BSIZE, ",\"ipfix:odid\":%" PRIu32, ntohl(hdr->odid));
    buffer_append(field);

    snprintf(field, LOCAL_BSIZE, ",\"ipfix:msgLength\":%" PRIu16, ntohs(hdr->length));
    buffer_append(field);

    if (m_src_addr) {
        buffer_append(",\"ipfix:srcAddr\":\"");
        buffer_append(m_src_addr);
        buffer_append("\"");
    }
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

        // Add detailed info to JSON string
        addDetailedInfo(hdr);

        // Add template ID to JSON string
        char field[LOCAL_BSIZE];
        snprintf(field, LOCAL_BSIZE, ",\"ipfix:templateId\":%" PRIu16, rec.tmplt->id);
        buffer_append(field);

        // Append the record with '}' parenthesis removed before
        buffer_append("}");
    }

     // Append the record with end of line character
     buffer_append("\n");
}