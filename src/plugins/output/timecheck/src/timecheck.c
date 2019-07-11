/**
 * \file src/plugins/output/timecheck/src/timecheck.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Time checker plugin for IPFIXcol 2
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

#include <ipfixcol2.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>

#include "config.h"
#include "../../../../core/message_ipfix.h"

/** Private Enterprise Number of standard IEs from IANA         */
#define PEN_IANA 0
/** Private Enterprise Number of Standard reverse IEs from IANA */
#define PEN_IANA_REV 29305

/** Plugin description */
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin type
    .type = IPX_PT_OUTPUT,
    // Plugin identification name
    .name = "timecheck",
    // Brief description of plugin
    .dsc = "The plugin checks that timestamp elements in flows are relatively recent.",
    // Configuration flags (reserved for future use)
    .flags = 0,
    // Plugin version string (like "1.2.3")
    .version = "2.0.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    .ipx_min = "2.0.0"
};

/** Instance */
struct instance_data {
    /** Parsed configuration of the instance   */
    struct instance_config *config;
    /** Current time (seconds since the Epoch) */
    uint64_t ts_now;

    /** Context reference (only for log!)      */
    ipx_ctx_t *ctx;
};

// Function prototype
static void
timestamp_check(const struct instance_data *inst, ipx_msg_ipfix_t *msg,
    const struct fds_drec_field *field);

int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params)
{
    // Create a private data
    struct instance_data *data = calloc(1, sizeof(*data));
    if (!data) {
        return IPX_ERR_DENIED;
    }

    if ((data->config = config_parse(ctx, params)) == NULL) {
        free(data);
        return IPX_ERR_DENIED;
    }

    data->ctx = ctx;
    ipx_ctx_private_set(ctx, data);
    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg)
{
    (void) ctx; // Suppress warnings

    struct instance_data *data = (struct instance_data *) cfg;
    config_destroy(data->config);
    free(data);
}

int
ipx_plugin_process(ipx_ctx_t *ctx, void *cfg, ipx_msg_t *msg)
{
    (void) ctx; // Suppress warnings
    struct instance_data *data = (struct instance_data *) cfg;
    ipx_msg_ipfix_t *ipfix_msg = ipx_msg_base2ipfix(msg);

    // Update the current UTC time
    data->ts_now = (uint64_t) time(NULL);

    // For each Data Record in the message
    uint32_t rec_cnt = ipx_msg_ipfix_get_drec_cnt(ipfix_msg);
    for (uint32_t i = 0; i < rec_cnt; ++i) {
        // For each field in the Data Record
        struct ipx_ipfix_record *rec = ipx_msg_ipfix_get_drec(ipfix_msg, i);
        struct fds_drec_iter it;

        fds_drec_iter_init(&it, &rec->rec, 0);
        while (fds_drec_iter_next(&it) != FDS_EOC) {
            // Is it a timestamp?
            const uint16_t field_id = it.field.info->id;
            const uint32_t field_en = it.field.info->en;

            if (field_en != PEN_IANA && field_en != PEN_IANA_REV) {
                // We don't check non-standard fields
                continue;
            }

            if (field_id < 150U || field_id > 157U) {
                // We want to check only IE elements within the range 150 - 157
                continue;
            }

            // Check if the value doesn't violate rules
            timestamp_check(data, ipfix_msg, &it.field);
        }
    }

    return IPX_OK;
}

/**
 * \brief Timestamp check function
 *
 * Only IANA fields flowStart... and flowEnd... are supported!
 * \param[in] inst  Plugin instance parameters
 * \param[in] msg   IPFIX Message from which the timestamp comes from
 * \param[in] field The timestamp to check
 */
void
timestamp_check(const struct instance_data *inst, ipx_msg_ipfix_t *msg,
    const struct fds_drec_field *field)
{
    assert((field->info->en == PEN_IANA || field->info->en == PEN_IANA_REV) && "Non-IANA PEN!");
    enum fds_iemgr_element_type elem_type;

    switch (field->info->id) {
    case 150U: // flowStartSeconds
    case 151U: // flowEndSeconds
        elem_type = FDS_ET_DATE_TIME_SECONDS;
        break;
    case 152U: // flowStartMilliseconds
    case 153U: // flowEndMilliseconds
        elem_type = FDS_ET_DATE_TIME_MILLISECONDS;
        break;
    case 154U: // flowStartMicroseconds
    case 155U: // flowEndMicroseconds
        elem_type = FDS_ET_DATE_TIME_MICROSECONDS;
        break;
    case 156U: // flowStartNanoseconds
    case 157U: // flowEndNanoseconds
        elem_type = FDS_ET_DATE_TIME_NANOSECONDS;
        break;
    default:
        assert(false && "Unhandled switch option!");
        return;
    }

    // Get the value
    uint64_t ts_value;
    int ret_code = fds_get_datetime_lp_be(field->data, field->size, elem_type, &ts_value);
    if (ret_code != FDS_OK) {
        IPX_CTX_WARNING(inst->ctx, "Timestamp conversion failed! Skipping...", '\0');
        return;
    }
    ts_value /= 1000U; // Convert from milliseconds to seconds (since the Epoch)

    // Check the deviation
    const char *violation_type;
    uint64_t ts_diff;

    if (ts_value <= inst->ts_now) {
        // Timestamps is from the past
        ts_diff = inst->ts_now - ts_value;
        if (ts_diff <= inst->config->dev_past) {
            return;
        }
        violation_type = "past";
    } else {
        // Timestamp is from the future
        ts_diff = ts_value - inst->ts_now;
        if (ts_diff <= inst->config->dev_future) {
            return;
        }
        violation_type = "future";
    }

    // Report the violation of rules
    const struct ipx_msg_ctx *msg_ctx = ipx_msg_ipfix_get_ctx(msg);
    const char *s_name = msg_ctx->session->ident;
    const uint32_t s_odid = msg->ctx.odid;

    uint16_t diff_secs = ts_diff % 60U;
    ts_diff /= 60U;
    uint16_t diff_mins = ts_diff % 60U;
    ts_diff /= 60U;
    uint16_t diff_hrs = ts_diff % 24U;
    ts_diff /= 24U;

    printf("%s [ODID: %"PRIu32"]: Timestamp (EN: %"PRIu32", ID: %"PRIu16") is "
        "%"PRIu64" days, %"PRIu16" hours, %"PRIu16" minutes and %"PRIu16" seconds in the %s "
        "(now: %"PRIu64", TS value: %"PRIu64" [seconds since the Epoch])\n",
        s_name, s_odid, field->info->en, field->info->id,
        ts_diff, diff_hrs, diff_mins, diff_secs, violation_type,
        inst->ts_now, ts_value);
}