/**
 * \file lnfstore.c
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \author Pavel Krobot <Pavel.Krobot@cesnet.cz>
 * \brief lnfstore plugin interface (source file)
 *
 * Copyright (C) 2015 - 2018 CESNET, z.s.p.o.
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
 * This software is provided ``as is, and any express or implied
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

#include "lnfstore.h"
#include "storage_common.h"

/** Plugin description */
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin type
    .type = IPX_PT_OUTPUT,
    // Plugin identification name
    .name = "lnfstore",
    // Brief description of plugin
    .dsc = "Output plugin that stores flow records in nfdump compatible files.",
    // Configuration flags (reserved for future use)
    .flags = 0,
    // Plugin version string (like "1.2.3")
    .version = "2.0.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    .ipx_min = "2.0.0"
};

// Storage plugin initialization function.
int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params) {
    // Process XML configuration
    struct conf_params *parsed_params = configuration_parse(ctx, params);
    if (!parsed_params) {
        IPX_CTX_ERROR(ctx, "Failed to parse the plugin configuration.", '\0');
        return IPX_ERR_DENIED;
    }

    // Create main plugin structure
    struct conf_lnfstore *conf = calloc(1, sizeof(*conf));
    if (!conf) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        configuration_free(parsed_params);
        return IPX_ERR_DENIED;
    }

    conf->params = parsed_params;

    // Init a LNF record for conversion from IPFIX
    if (lnf_rec_init(&conf->record.rec_ptr) != LNF_OK) {
        IPX_CTX_ERROR(ctx, "Failed to initialize an internal structure for conversion of records",
            '\0');
        configuration_free(parsed_params);
        free(conf);
        return IPX_ERR_DENIED;
    }

    conf->record.translator = translator_init(ctx);
    if (!conf->record.translator) {
        IPX_CTX_ERROR(ctx, "Failed to initialize a record translator.", '\0');
        lnf_rec_free(conf->record.rec_ptr);
        configuration_free(parsed_params);
        free(conf);
        return IPX_ERR_DENIED;
    }

    // Init basic/profile file storage
    //if (conf->params->profiles.en) {
    //    conf->storage.profiles = stg_profiles_create(parsed_params);
    //} else {
        conf->storage.basic = stg_basic_create(ctx, parsed_params);
    //}

    if (!conf->storage.basic/* && !conf->storage.profiles*/) {
        IPX_CTX_ERROR(ctx, "Failed to initialize an internal structure for file storage(s).", '\0');
        translator_destroy(conf->record.translator);
        lnf_rec_free(conf->record.rec_ptr);
        configuration_free(parsed_params);
        free(conf);
    }

    // Save the configuration
    ipx_ctx_private_set(ctx, conf);
    return IPX_OK;
}

// Pass IPFIX data with supplemental structures into the storage plugin.
int
ipx_plugin_process(ipx_ctx_t *ctx, void *cfg, ipx_msg_t *msg)
{
    (void) ctx;
    struct conf_lnfstore *conf = (struct conf_lnfstore *) cfg;

    // Decide whether close files and create new time window
    time_t now = time(NULL);
    if (difftime(now, conf->window_start) > conf->params->window.size) {
        time_t new_time = now;

        if (conf->params->window.align) {
            // We expect that new_time is integer
            new_time /= conf->params->window.size;
            new_time *= conf->params->window.size;
        }

        conf->window_start = new_time;

        // Update storage files
        stg_basic_new_window(conf->storage.basic, new_time);
    }

    ipx_msg_ipfix_t *ipfix = ipx_msg_base2ipfix(msg);
    const uint32_t rec_cnt = ipx_msg_ipfix_get_drec_cnt(ipfix);
    for (uint32_t i = 0; i < rec_cnt; i++) {
        // Get a pointer to the next record
        struct ipx_ipfix_record *ipfix_rec = ipx_msg_ipfix_get_drec(ipfix, i);
        bool biflow = (ipfix_rec->rec.tmplt->flags & FDS_TEMPLATE_BIFLOW) != 0;

        // Fill record
        uint16_t flags = biflow ? FDS_DREC_BIFLOW_FWD : 0; // In case of biflow, forward fields only
        lnf_rec_t *lnf_rec = conf->record.rec_ptr;
        if (translator_translate(conf->record.translator, &ipfix_rec->rec, lnf_rec, flags) <= 0) {
            // Nothing to store
            continue;
        }

        stg_basic_store(conf->storage.basic, lnf_rec);

        // Is it biflow? Store the reverse direction
        if (!biflow) {
            continue;
        }

        flags = FDS_DREC_BIFLOW_REV;
        if (translator_translate(conf->record.translator, &ipfix_rec->rec, lnf_rec, flags) <= 0) {
            // Nothing to store
            continue;
        }

        stg_basic_store(conf->storage.basic, lnf_rec);
    }

    return 0;
}

// Storage plugin "destructor"
void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg)
{
    (void) ctx;
    struct conf_lnfstore *conf = (struct conf_lnfstore *) cfg;

    // Destroy mode resources
    //if (conf->params->profiles.en) {
    //    stg_profiles_destroy(conf->storage.profiles);
    //} else {
        stg_basic_destroy(conf->storage.basic);
    //}

    // Destroy a translator and a record
    translator_destroy(conf->record.translator);
    lnf_rec_free(conf->record.rec_ptr);

    // Destroy parsed XML configuration
    configuration_free(conf->params);

    // Destroy instance structure
    free(conf);
}
