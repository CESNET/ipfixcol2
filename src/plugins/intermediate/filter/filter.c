/**
 * \file src/plugins/intermediate/filter/filter.c
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief The filter plugin implementation
 * \date 2020
 */

/* Copyright (C) 2020 CESNET, z.s.p.o.
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

#include <libfds.h>
#include <ipfixcol2.h>
#include <stdlib.h>
#include <arpa/inet.h> // ntohs
#include <endian.h> // be64toh
#include <assert.h>
#include <time.h> // timespec
#include <stdbool.h>

#include <stdio.h>

#include "config.h"
#include "msg_builder.h"

IPX_API struct ipx_plugin_info ipx_plugin_info = {
    .type = IPX_PT_INTERMEDIATE,
    .name = "filter",
    .dsc = "Data record filtering plugin",
    .flags = 0,
    .version = "0.0.1",
    .ipx_min = "2.0.0"
};

struct plugin_ctx {
    struct config *config;
    fds_ipfix_filter_t *filter;
    ipx_ctx_t *ipx_ctx;
};

struct plugin_ctx *
create_plugin_ctx()
{
    struct plugin_ctx *pctx = calloc(1, sizeof(struct plugin_ctx));
    if (!pctx) {
        return NULL;
    }
    return pctx;
}

void
destroy_plugin_ctx(struct plugin_ctx *pctx)
{
    if (!pctx) {
        return;
    }
    config_destroy(pctx->config);
    fds_ipfix_filter_destroy(pctx->filter);
    free(pctx);
}

static inline bool
record_belongs_to_set(struct fds_ipfix_set_hdr *set, struct fds_drec *record)
{
    uint8_t *set_begin = (uint8_t *) set;
    uint8_t *set_end = set_begin + ntohs(set->length);
    uint8_t *record_begin = record->data;

    return record_begin >= set_begin && record_begin < set_end;
}


int
ipx_plugin_init(ipx_ctx_t *ipx_ctx, const char *params)
{
    // Create the plugin context
    struct plugin_ctx *pctx = create_plugin_ctx();
    if (!pctx) {
        return IPX_ERR_DENIED;
    }

    pctx->ipx_ctx = ipx_ctx;

    // Parse config
    pctx->config = config_parse(ipx_ctx, params);
    if (!pctx->config) {
        destroy_plugin_ctx(pctx);
        return IPX_ERR_DENIED;
    }

    // Create the opts
    int rc = fds_ipfix_filter_create(&pctx->filter,
        ipx_ctx_iemgr_get(ipx_ctx), pctx->config->expr);

    if (rc != FDS_OK) {
        const char *error = fds_ipfix_filter_get_error(pctx->filter);
        IPX_CTX_ERROR(ipx_ctx, "Error creating filter: %s", error);
        destroy_plugin_ctx(pctx);
        return IPX_ERR_DENIED;
    }

    ipx_ctx_private_set(ipx_ctx, pctx);
    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ipx_ctx, void *data)
{
    (void) ipx_ctx;
    destroy_plugin_ctx(data);
}

int
ipx_plugin_process(ipx_ctx_t *ipx_ctx, void *data, ipx_msg_t *base_msg)
{
    // We only care about IPFIX messages
    if (ipx_msg_get_type(base_msg) != IPX_MSG_IPFIX) {
        ipx_ctx_msg_pass(ipx_ctx, base_msg);
        return IPX_OK;
    }

    struct plugin_ctx *pctx = (struct plugin_ctx *) data;

    // Get the ipfix message
    ipx_msg_ipfix_t *orig_msg = ipx_msg_base2ipfix(base_msg);

    // Initialize message builder
    msg_builder_s mb;
    int rc = msg_builder_init(&mb, ipx_ctx, orig_msg);
    if (rc != IPX_OK) {
        IPX_CTX_ERROR(ipx_ctx, "Error initializing message builder");
        return rc;
    }

    // Get ipfix sets
    struct ipx_ipfix_set *sets;
    size_t set_cnt;
    ipx_msg_ipfix_get_sets(orig_msg, &sets, &set_cnt);

    IPX_CTX_DEBUG(ipx_ctx, "Processing IPFIX message (%d sets)", set_cnt);


    // Build new message
    size_t drec_idx = 0;
    for (size_t set_idx = 0; set_idx < set_cnt; set_idx++) {
        IPX_CTX_DEBUG(ipx_ctx, "Processing set %d", set_idx);

        struct ipx_ipfix_set set = sets[set_idx];
        uint16_t set_id = ntohs(set.ptr->flowset_id);

        // If it's not a data set simply copy it over
        if (set_id < FDS_IPFIX_SET_MIN_DSET) {
            rc = msg_builder_copy_set(&mb, &set);
            if (rc != IPX_OK) {
                IPX_CTX_ERROR(ipx_ctx, "Error copying set");
                ipx_msg_ipfix_destroy(mb.msg);
                return rc;
            }
            continue;
        }

        msg_builder_begin_dset(&mb, set_id);

        // If it's a data set copy over all its data records that pass the filter
        for (;;) {
            struct ipx_ipfix_record *drec = ipx_msg_ipfix_get_drec(orig_msg, drec_idx);

            if (drec == NULL || !record_belongs_to_set(set.ptr, &drec->rec)) {
                break;
            }

            if (fds_ipfix_filter_eval_biflow(pctx->filter, &drec->rec) != FDS_IPFIX_FILTER_NO_MATCH) {
                rc = msg_builder_copy_drec(&mb, drec);
                if (rc != IPX_OK) {
                    IPX_CTX_ERROR(ipx_ctx, "Error copying data record");
                    ipx_msg_ipfix_destroy(mb.msg);
                    return rc;
                }
            }

            drec_idx++;
        }


        rc = msg_builder_end_dset(&mb);
        if (rc != IPX_OK) {
            IPX_CTX_ERROR(ipx_ctx, "Error ending data set");
            ipx_msg_ipfix_destroy(mb.msg);
            return rc;
        }
    }
    assert(drec_idx == ipx_msg_ipfix_get_drec_cnt(orig_msg));

    // Finish building the message
    msg_builder_finish(&mb);

    // Destroy the original
    ipx_msg_ipfix_destroy(orig_msg);

    // If the message is empty throw it away
    if (msg_builder_is_empty_msg(&mb)) {
        ipx_msg_ipfix_destroy(mb.msg);
    } else {
        ipx_ctx_msg_pass(ipx_ctx, ipx_msg_ipfix2base(mb.msg));
    }

    return IPX_OK;
}