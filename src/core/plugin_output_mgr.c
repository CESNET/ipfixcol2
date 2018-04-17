/**
 * \file src/core/plugin_output_mgr.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Internal output manager plugin (source file)
 * \date 2018
 */

/* Copyright (C) 2018 CESNET, z.s.p.o.
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

#include <stdlib.h>
#include <stddef.h>
#include "plugin_output_mgr.h"
#include "message_base.h"
#include "context.h"

/** Definition of a connection with an output instance      */
struct ipx_output_mgr_rec {
    /** Ring buffer connection (writer only)                */
    ipx_ring_t *ring;
    /** Type of filter                                      */
    enum ipx_odid_filter_type type;
    /** ODID filter (NULL if #type == IPX_ODID_FILTER_NONE) */
    const ipx_orange_t *odid_filter;
};

/** List of output destinations */
struct ipx_output_mgr_list {
    /** Number of output instances */
    size_t size;
    /** Array of records           */
    struct ipx_output_mgr_rec *recs;
};

ipx_output_mgr_list_t *
ipx_output_mgr_list_create()
{
    struct ipx_output_mgr_list *result = calloc(1, sizeof(*result));
    if (!result) {
        return NULL;
    }

    result->size = 0;
    result->recs = NULL;
    return result;
}

void
ipx_output_mgr_list_destroy(ipx_output_mgr_list_t *list)
{
    free(list->recs);
    free(list);
}

bool
ipx_output_mgr_list_empty(const ipx_output_mgr_list_t *list)
{
    return (list->size == 0);
}

int
ipx_output_mgr_list_add(ipx_output_mgr_list_t *list, ipx_ring_t *ring,
    enum ipx_odid_filter_type odid_type, const ipx_orange_t *odid_filter)
{
    // Check arguments
    if (list == NULL || ring == NULL) {
        return IPX_ERR_ARG;
    }

    if (odid_type != IPX_ODID_FILTER_NONE && odid_filter == NULL) {
        // The ODID filter is missing
        return IPX_ERR_ARG;
    }

    // Add a new record
    size_t new_size = list->size + 1;
    size_t recs_size = new_size * sizeof(struct ipx_output_mgr_rec);
    struct ipx_output_mgr_rec *new_recs = realloc(list->recs, recs_size);
    if (!new_recs) {
        return IPX_ERR_NOMEM;
    }

    list->size = new_size;
    list->recs = new_recs;

    struct ipx_output_mgr_rec *rec = &list->recs[new_size - 1];
    rec->ring = ring;
    rec->type = odid_type;
    rec->odid_filter = odid_filter;
    return IPX_OK;
}

// ------------------------------------------------------------------------------------------------

const struct ipx_plugin_info ipx_plugin_output_mgr_info = {
    .name    = "Output manager",
    .dsc     = "Internal IPFIXcol plugin for passing messages to output plugins.",
    .type    = IPX_PT_OUTPUT_MGR,
    .flags   = 0,
    .version = "1.0.0",
    .ipx_min = "2.0.0"
};

int
ipx_plugin_output_mgr_init(ipx_ctx_t *ctx, const char *params)
{
    (void) params;

    // Check that all message types are subscribed
    ipx_msg_mask_t mask = IPX_MSG_MASK_ALL;
    if (ipx_ctx_subscribe(ctx, &mask, NULL) != IPX_OK) {
        IPX_CTX_ERROR(ctx, "Unable to subscribe to all message types!", '\0');
        return IPX_ERR_DENIED;
    }

    return IPX_OK;
}

void
ipx_plugin_output_mgr_destroy(ipx_ctx_t *ctx, void *cfg)
{
    // Do nothing, private data should be freed by the configurator
    (void) ctx;
    (void) cfg;
}

int
ipx_plugin_output_mgr_process(ipx_ctx_t *ctx, void *cfg, ipx_msg_t *msg)
{
    (void) ctx;
    // List of output destination is prepared by the configurator
    struct ipx_output_mgr_list *list = (struct ipx_output_mgr_list *) cfg;
    assert(list != NULL);

    // Only IPFIX messages are filtered
    enum ipx_msg_type msg_type = ipx_msg_get_type(msg);
    if (msg_type != IPX_MSG_IPFIX) {
        // Set the number of references and pass the message to all output instances
        ipx_msg_header_cnt_set(msg, (unsigned int) list->size);

        for (size_t i = 0; i < list->size; ++i) {
            ipx_ring_push(list->recs[i].ring, msg);
        }

        return IPX_OK;
    }

    // First, get number of destinations...
    uint64_t dest_mask = 0;
    unsigned int dest_cnt = 0;
    uint32_t odid = ipx_msg_ipfix_get_ctx(ipx_msg_base2ipfix(msg))->odid;

    for (size_t i = 0; i < list->size; ++i) {
        struct ipx_output_mgr_rec *rec = &list->recs[i];
        switch (rec->type) {
        case IPX_ODID_FILTER_NONE:
            // Add to the destinations
            break;
        case IPX_ODID_FILTER_ONLY:
            if (ipx_orange_in(rec->odid_filter, odid)) {
                break;    // Add to the destinations
            } else {
                continue; // Skip
            }
        case IPX_ODID_FILTER_EXCEPT:
            if (ipx_orange_in(rec->odid_filter, odid)) {
                continue; // Skip
            } else {
                break;    // Add to the destinations
            }
        }

        dest_mask |= (1ULL << i);
        dest_cnt++;
    }

    if (dest_cnt == 0) {
        // No-one wants the message -> destroy
        ipx_msg_ipfix_destroy(ipx_msg_base2ipfix(msg));
        return IPX_OK;
    }

    // Set the number of references and send to all selected destinations
    ipx_msg_header_cnt_set(msg, dest_cnt);
    for (size_t dest_idx = 0; dest_mask != 0; dest_idx++, dest_mask >>= 1) {
        if ((dest_mask & 0x1) == 0) {
            // Skip
            continue;
        }

        struct ipx_output_mgr_rec *rec = &list->recs[dest_idx];
        ipx_ring_push(rec->ring, msg);
    }

    return IPX_OK;
}