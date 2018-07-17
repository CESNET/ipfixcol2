/**
 * \file src/plugins/intermediate/anonymization/anonymization.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief IPv4/IPv6 address anonymization plugin for IPFIXcol2
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

#include <ipfixcol2.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#include "config.h"
#include "Crypto-PAn/panonymizer.h"

/** Plugin description */
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin type
    .type = IPX_PT_INTERMEDIATE,
    // Plugin identification name
    .name = "anonymization",
    // Brief description of plugin
    .dsc = "IPv4/IPv6 address anonymization plugin",
    // Configuration flags (reserved for future use)
    .flags = 0,
    // Plugin version string (like "1.2.3")
    .version = "2.0.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    .ipx_min = "2.0.0"
};

/** Instance */
struct instance_data {
    /** Parsed configuration of the instance  */
    struct anon_config *config;
};

/**
 * \brief Anonymize an IPv4/IPv6 address by setting lower half of the address to be zeros
 * \param field IPFIX field with an address to anonymize
 */
static void
anonymize_trunc(struct fds_drec_field *field)
{
    // IP addresses are stored in Network byte order
    if (field->size == 4) {
        memset(&field->data[2], 0, 2);
        return;
    }

    if (field->size == 16) {
        memset(&field->data[8], 0, 8);
        return;
    }
}

/**
 * \brief Anonymize an IPv4/IPV6 address using Crypto-PAn anonymization technique
 * \param[in] field IPFIX field with an address to anonymize
 */
static void
anonymize_cryptopan(struct fds_drec_field *field)
{
    if (field->size == 4) {
        uint32_t *mem = (uint32_t *) field->data;
        (*mem) = htonl(anonymize(ntohl(*mem)));
        return;
    }

    if (field->size == 16) {
        uint64_t addr_orig[2];
        uint64_t addr_anon[2];
        memcpy(addr_orig, field->data, field->size);
        anonymize_v6(addr_orig, addr_anon);
        memcpy(field->data, addr_anon, field->size);
        return;
    }
}

// -------------------------------------------------------------------------------------------------

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

    if (data->config->mode == AN_CRYPTOPAN) {
        PAnonymizer_Init((uint8_t *)data->config->crypto_key);
    }

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
    struct instance_data *data = (struct instance_data *) cfg;

    // Process all data records in the IPFIX message
    ipx_msg_ipfix_t *ipfix_msg = ipx_msg_base2ipfix(msg);
    const uint32_t rec_cnt = ipx_msg_ipfix_get_drec_cnt(ipfix_msg);
    for (uint32_t i = 0; i < rec_cnt; ++i) {
        struct ipx_ipfix_record *rec = ipx_msg_ipfix_get_drec(ipfix_msg, i);

        // Go through the record and anonymize all IPv4/IPv6 addresses
        struct fds_drec_iter it;
        fds_drec_iter_init(&it, &rec->rec, 0);

        while (fds_drec_iter_next(&it) != FDS_EOC) {
            const struct fds_tfield *info = it.field.info;
            if (info->def == NULL) {
                // Skip unknown fields
                continue;
            }

            const enum fds_iemgr_element_type type = info->def->data_type;
            if (type != FDS_ET_IPV4_ADDRESS && type != FDS_ET_IPV6_ADDRESS) {
                // Not an IPv4/IPv6 address
                continue;
            }

            if (it.field.size != 4U && it.field.size != 16U) {
                IPX_CTX_DEBUG(ctx, "Unable to anonymize an IP address with invalid size "
                    "(%" PRIu16 "bytes)!", it.field.size);
                continue;
            }

            if (data->config->mode == AN_TRUNC) {
                // Truncate the address
                anonymize_trunc(&it.field);
            } else {
                // Crypto-PAn
                anonymize_cryptopan(&it.field);
            }
        }
    }

    // Always pass the message
    ipx_ctx_msg_pass(ctx, msg);
    return IPX_OK;
}
