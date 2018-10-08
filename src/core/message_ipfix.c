/**
 * \file src/core/message_ipfix.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief IPFIX Message (source file)
 * \date 2016-2018
 */

/* Copyright (C) 2016 CESNET, z.s.p.o.
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

#include <ipfixcol2.h>
#include <libfds.h>
#include "message_base.h"
#include "message_ipfix.h"
#include "context.h"

#include <stddef.h> // offsetof
#include <stdlib.h> // free

// Check correctness of structure implementation
static_assert(offsetof(struct ipx_msg_ipfix, msg_header.type) == 0,
    "Message header must be the first element of each IPFIXcol message.");

size_t
ipx_msg_ipfix_size(uint32_t rec_cnt, size_t rec_size)
{
    return offsetof(struct ipx_msg_ipfix, recs) + (rec_cnt * rec_size);
}

ipx_msg_ipfix_t *
ipx_msg_ipfix_create(const ipx_ctx_t *plugin_ctx, const struct ipx_msg_ctx *msg_ctx,
    uint8_t *msg_data)
{
    const size_t rec_size = ipx_ctx_recsize_get(plugin_ctx);
    const size_t new_size = ipx_msg_ipfix_size(REC_DEF_CNT, rec_size);
    struct ipx_msg_ipfix *wrapper = calloc(1, new_size);
    if (!wrapper) {
        return NULL;
    }

    ipx_msg_header_init(&wrapper->msg_header, IPX_MSG_IPFIX);
    wrapper->ctx = *msg_ctx;
    wrapper->raw_pkt = msg_data;
    wrapper->sets.cnt_alloc = SET_DEF_CNT;
    wrapper->rec_info.cnt_alloc = REC_DEF_CNT;
    wrapper->rec_info.rec_size = rec_size;
    return wrapper;
}

void
ipx_msg_ipfix_destroy(ipx_msg_ipfix_t *msg)
{
    // Destroy the IPFIX packet
    free(msg->raw_pkt);

    // Destroy the wrapper
    if (msg->sets.extended) {
        free(msg->sets.extended);
    }
    ipx_msg_header_destroy((ipx_msg_t *) msg);
    free(msg);
}

uint8_t *
ipx_msg_ipfix_get_packet(ipx_msg_ipfix_t *msg)
{
    return msg->raw_pkt;
}

struct ipx_msg_ctx *
ipx_msg_ipfix_get_ctx(ipx_msg_ipfix_t *msg)
{
    return &msg->ctx;
}

void
ipx_msg_ipfix_get_sets(ipx_msg_ipfix_t *msg, struct ipx_ipfix_set **sets, size_t *size)
{
    if (sets != NULL) {
        if (msg->sets.cnt_valid <= SET_DEF_CNT) {
            assert(msg->sets.extended == NULL);
            *sets = msg->sets.base;
        } else {
            *sets = msg->sets.extended;
        }
    }

    if (size != NULL) {
        *size = msg->sets.cnt_valid;
    }
}

uint32_t
ipx_msg_ipfix_get_drec_cnt(const ipx_msg_ipfix_t *msg)
{
    return msg->rec_info.cnt_valid;
}

struct ipx_ipfix_record *
ipx_msg_ipfix_get_drec(ipx_msg_ipfix_t *msg, uint32_t idx)
{
    if (idx >= msg->rec_info.cnt_valid) {
        return NULL;
    }

    const size_t offset = idx * msg->rec_info.rec_size;
    uint8_t *rec_start = ((uint8_t *) msg->recs) + offset;
    return (struct ipx_ipfix_record *) rec_start;
}

struct ipx_ipfix_set *
ipx_msg_ipfix_add_set_ref(struct ipx_msg_ipfix *msg)
{
    if (msg->sets.cnt_valid < SET_DEF_CNT) {
        // Return a reference into "base" array
        return &msg->sets.base[msg->sets.cnt_valid++];
    }

    if (msg->sets.cnt_valid == msg->sets.cnt_alloc) {
        const uint32_t alloc_new =  2U * msg->sets.cnt_alloc;
        const size_t alloc_size = alloc_new * sizeof(struct ipx_ipfix_set);
        msg->sets.extended = realloc(msg->sets.extended, alloc_size);
        if (!msg->sets.extended) {
            return NULL;
        }

        // Move sets from base to extended array
        if (msg->sets.cnt_valid == SET_DEF_CNT) {
            const size_t copy_size = msg->sets.cnt_valid * sizeof(struct ipx_ipfix_set);
            memcpy(msg->sets.extended, msg->sets.base, copy_size);
        }
        msg->sets.cnt_alloc = alloc_new;
    }

    // Return reference into "extended" array
    return &msg->sets.extended[msg->sets.cnt_valid++];
}

struct ipx_ipfix_record *
ipx_msg_ipfix_add_drec_ref(struct ipx_msg_ipfix **msg_ref)
{
    struct ipx_msg_ipfix *msg = *msg_ref;
    if (msg->rec_info.cnt_valid == msg->rec_info.cnt_alloc) {
        // Reallocation of the message is necessary
        const uint32_t alloc_new = 2U * msg->rec_info.cnt_valid;
        const size_t alloc_size = ipx_msg_ipfix_size(alloc_new, msg->rec_info.rec_size);
        struct ipx_msg_ipfix *msg_new = realloc(msg, alloc_size);
        if (!msg_new) {
            return NULL;
        }

        msg_new->rec_info.cnt_alloc = alloc_new;
        msg = msg_new;
        *msg_ref = msg_new;
    }

    assert(msg->rec_info.cnt_valid < msg->rec_info.cnt_alloc);
    const size_t offset = msg->rec_info.cnt_valid * msg->rec_info.rec_size;
    msg->rec_info.cnt_valid++;
    return ((struct ipx_ipfix_record *) (((uint8_t *) msg->recs) + offset));
}