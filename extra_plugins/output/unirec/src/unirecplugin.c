/**
 * \file unirecstore.c
 * \author Tomas Cejka <cejkat@cesnet.cz>
 * \author Jaroslav Hlavac <hlavaj20@fit.cvut.cz>
 * \brief unirecstore plugin interface (source file)
 *
 * Copyright (C) 2018 CESNET, z.s.p.o.
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
#include <libtrap/trap.h>
#include <stdio.h>
#include <pthread.h>
#include <unirec/unirec.h>

#include "translator.h"
#include "configuration.h"
#include "map.h"

/** Filename of IPFIX-to-UniRec                                */
#define CONF_FILENAME "unirec-elements.txt"
/** Name of TRAP context that belongs to the plugin            */
#define PLUGIN_TRAP_NAME "IPFIXcol2-UniRec"
/** Description of the TRAP context that belongs to the plugin */
#define PLUGIN_TRAP_DSC  "UniRec output plugin for IPFIXcol2."

/** GLOBAL mutex shared across all plugin instances  */
static pthread_mutex_t urp_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * GLOBAL UniRec plugin instance counter
 *
 * This counter represents number of active instances (threads) of this plugin.
 * It must be incremented in init() and decremented in destroy().
 */
static uint8_t instance_cnt = 0;

/** Plugin description */
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin type
    .type = IPX_PT_OUTPUT,
    // Plugin identification name
    .name = "unirec",
    // Brief description of plugin
    .dsc = "Output plugin that sends flow records in UniRec format via TRAP communication "
        "interface (into NEMEA modules).",
    // Configuration flags (reserved for future use)
    .flags = 0,
    // Plugin version string (like "1.2.3")
    .version = "2.2.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    .ipx_min = "2.0.0"
};

/**
 * \brief Plugin instance structure
 */
struct conf_unirec {
    /** Parser configuration from XML file   */
    struct conf_params *params;
    /** TRAP context                         */
    trap_ctx_t *trap_ctx;
    /** UniRec template                      */
    ur_template_t *ur_tmplt;
    /** IPFIX to UniRec translator           */
    translator_t *trans;
};

/**
 * \brief Get the IPFIX-to-UniRec conversion database
 * \param ctx Plugin context
 * \return Conversion table or NULL (an error has occurred)
 */
static map_t *
ipfix2unirec_db(ipx_ctx_t *ctx)
{
    const char *path = ipx_api_cfg_dir();
    const size_t full_size = strlen(path) + strlen(CONF_FILENAME) + 2; // 2 = '/' + '\0'
    char *full_path = malloc(full_size * sizeof(char));
    if (!full_path) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        return NULL;
    }

    int ret_val = snprintf(full_path, full_size, "%s/%s", path, CONF_FILENAME);
    if (ret_val < 0 || ((size_t) ret_val) >= full_size) {
        IPX_CTX_ERROR(ctx, "Failed to generate a configuration path (internal error)", '\0');
        free(full_path);
        return NULL;
    }

    map_t *map = map_init(ipx_ctx_iemgr_get(ctx));
    if (!map) {
        IPX_CTX_ERROR(ctx, "Failed to initialize conversion map! (%s:%d)", __FILE__, __LINE__);
        free(full_path);
        return NULL;
    }

    if (map_load(map, full_path) != IPX_OK) {
        IPX_CTX_ERROR(ctx, "Failed to initialize conversion database: %s", map_last_error(map));
        map_destroy(map);
        free(full_path);
        return NULL;
    }

    free(full_path);
    return map;
}

/**
 * \brief Initialize core components of the plugin (internal function)
 * \warning The function expects that the mutex shared across Unirec plugin instances is locked!
 * \param[in] ctx Plugin context (just for log)
 * \param[in] cfg Plugin configuration (will be updated)
 * \param[in] map IPFIX-to-UniRec conversion database
 * \return #IPX_OK on success
 * \return #IPX_ERR_NOMEM or #IPX_ERR_DENIED in case of failure
 */
static int
core_initialize_inter(ipx_ctx_t *ctx, struct conf_unirec *cfg, const map_t *map)
{
    // Register all UniRec fields
    IPX_CTX_INFO(ctx, "UniRec fields definition.", '\0');
    for (size_t i = 0; i < map_size(map); ++i) {
        const struct map_rec *rec = map_get(map, i);
        int rc = ur_define_field(rec->unirec.name, rec->unirec.type);
        if (rc == UR_E_MEMORY) {
            IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
            return IPX_ERR_NOMEM;
        } else if (rc == UR_E_INVALID_NAME) {
            IPX_CTX_ERROR(ctx, "Unable to define UniRec field '%s': Invalid name!",
                rec->unirec.name);
            return IPX_ERR_DENIED;
        } else if (rc == UR_E_TYPE_MISMATCH) {
            IPX_CTX_ERROR(ctx, "Unable to define UniRec field '%s': The name already exists, "
                "but the type is different!", rec->unirec.name);
            return IPX_ERR_DENIED;
        }
    }

    // Create a TRAP interface
    const char *ifc_spec = cfg->params->trap_ifc_spec;
    IPX_CTX_INFO(ctx, "Initialization of TRAP with IFCSPEC: '%s'.", ifc_spec);

    const char *instance_name = ipx_ctx_name_get(ctx);
    cfg->trap_ctx = trap_ctx_init3(PLUGIN_TRAP_NAME, PLUGIN_TRAP_DSC, 0, 1, ifc_spec, instance_name);
    if (!cfg->trap_ctx) {
        IPX_CTX_ERROR(ctx, "Failed to initialize TRAP (%s:%d)", __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }

    if (trap_ctx_get_last_error(cfg->trap_ctx) != TRAP_E_OK) {
        IPX_CTX_ERROR(ctx, "Failed to initialize TRAP: %s",
            trap_ctx_get_last_error_msg(cfg->trap_ctx), __FILE__, __LINE__);
        trap_ctx_finalize(&cfg->trap_ctx);
        return IPX_ERR_DENIED;
    }

    // Create a UniRec template and set it as TRAP output template
    const char *tmplt_str = cfg->params->unirec_fmt;
    IPX_CTX_INFO(ctx, "Initialization of UniRec template: '%s'", tmplt_str);
    char *err_str = NULL;
    cfg->ur_tmplt = ur_ctx_create_output_template(cfg->trap_ctx, 0, tmplt_str, &err_str);
    if (!cfg->ur_tmplt) {
        IPX_CTX_ERROR(ctx, "Failed to create UniRec template '%s': '%s'", tmplt_str, err_str);
        trap_ctx_finalize(&cfg->trap_ctx);
        free(err_str);
        return IPX_ERR_DENIED;
    }

    char *ur_tmplt_str = ur_template_string_delimiter(cfg->ur_tmplt, ',');
    IPX_CTX_INFO(ctx, "Using the following created UniRec template: '%s'", ur_tmplt_str);
    free(ur_tmplt_str);

    // Prepare a translator
    const char *tmplt_spec = cfg->params->unirec_spec;
    IPX_CTX_INFO(ctx, "Initialization of IPFIX to UniRec translator: '%s'", tmplt_spec );
    cfg->trans = translator_init(ctx, map, cfg->ur_tmplt, tmplt_spec);
    if (!cfg->trans) {
        IPX_CTX_ERROR(ctx, "Failed to initialize IPFIX to UniRec translator.", '\0');
        trap_ctx_finalize(&cfg->trap_ctx);
        ur_free_template(cfg->ur_tmplt); // Template MUST be destroyed after the TRAP ifc!
        return IPX_ERR_DENIED;
    }

    return IPX_OK;
}

/**
 * \brief Initialize core components of the plugin
 *
 * The function will register all UniRec fields defined in IPFIX-to-UniRec conversion database,
 * create a TRAP output interface, create an output template and IPFIX-to-UniRec translator.
 * \param[in] ctx Plugin context (just for log)
 * \param[in] cfg Plugin configuration (will be updated)
 * \param[in] map IPFIX-to-UniRec conversion database
 * \return #IPX_OK on success
 * \return #IPX_ERR_NOMEM or #IPX_ERR_DENIED in case of failure
 */
static int
core_initialize(ipx_ctx_t *ctx, struct conf_unirec *cfg, const map_t *map)
{
    int tmp;
    if ((tmp = pthread_mutex_lock(&urp_mutex)) != 0) {
        IPX_CTX_ERROR(ctx, "Failed to initialize core components (TRAP, UniRec, etc.): "
            "pthread_mutex_lock() failed with return code '%d' (%s:%d)", tmp, __FILE__, __LINE__);
        return IPX_ERR_DENIED;
    }

    IPX_CTX_INFO(ctx, "Constructor of core components called!", '\0');
    int ret_code = core_initialize_inter(ctx, cfg, map);
    if (ret_code == IPX_OK) {
        // Success -> increment number of instances
        instance_cnt++;
    } else {
        // Failed!
        if (instance_cnt == 0) {
            // This is the only running instance -> destroy possibly registered UniRec fields
            IPX_CTX_INFO(ctx, "Removing all defined UniRec fields", '\0');
            ur_finalize();
        }
    }

    if ((tmp = pthread_mutex_unlock(&urp_mutex)) != 0) {
        IPX_CTX_ERROR(ctx, "pthread_mutex_unlock() failed with return code '%d' (%s:%d)",
            tmp,  __FILE__, __LINE__);
    }

    return ret_code;
}

/**
 * \brief Destroy initialized core components
 * \param[in] ctx Plugin context (just for log)
 * \param[in] cfg Plugin configuration (will be updated)
 */
static void
core_destroy(ipx_ctx_t *ctx, struct conf_unirec *cfg)
{
    int tmp;
    if ((tmp = pthread_mutex_lock(&urp_mutex)) != 0) {
        IPX_CTX_ERROR(ctx, "Failed to destroy core components (TRAP, UniRec, etc.): "
            "pthread_mutex_lock() failed with return code '%d' (%s:%d)", tmp, __FILE__, __LINE__);
        return;
    }

    IPX_CTX_INFO(ctx, "Destructor of core components called!", '\0');
    instance_cnt--;

    translator_destroy(cfg->trans);
    trap_ctx_finalize(&cfg->trap_ctx);
    ur_free_template(cfg->ur_tmplt); // Template MUST be destroyed after the TRAP ifc!
    if (instance_cnt == 0) {
        IPX_CTX_INFO(ctx, "Removing all defined UniRec fields", '\0');
        ur_finalize();
    }

    if ((tmp = pthread_mutex_unlock(&urp_mutex)) != 0) {
        IPX_CTX_ERROR(ctx, "pthread_mutex_unlock() failed with return code '%d' (%s:%d)",
            tmp,  __FILE__, __LINE__);
    }

    cfg->ur_tmplt = NULL;
    cfg->trap_ctx = NULL;
    cfg->trans = NULL;
}

// Output plugin initialization function
int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params)
{
    // Process XML configuration
    struct conf_params *parsed_params = configuration_parse(ctx, params);
    if (!parsed_params) {
        IPX_CTX_ERROR(ctx, "Failed to parse the plugin configuration.", '\0');
        return IPX_ERR_DENIED;
    }

    // Create main plugin structure
    struct conf_unirec *conf = calloc(1, sizeof(*conf));
    if (!conf) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        configuration_free(parsed_params);
        return IPX_ERR_DENIED;
    }
    conf->params = parsed_params;

    // Load IPFIX-to-UniRec conversion database
    map_t *conv_db = ipfix2unirec_db(ctx);
    if (!conv_db) {
        configuration_free(parsed_params);
        free(conf);
        return IPX_ERR_DENIED;
    }

    // Initialize core components
    if (core_initialize(ctx, conf, conv_db) != IPX_OK) {
        map_destroy(conv_db);
        configuration_free(parsed_params);
        free(conf);
        return IPX_ERR_DENIED;
    }

    // Success
    map_destroy(conv_db); // Destroy the mapping database (we don't need it anymore)
    ipx_ctx_private_set(ctx, conf);
    return IPX_OK;
}

// Output plugin destruction function
void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg)
{
    struct conf_unirec *conf = (struct conf_unirec *) cfg;
    core_destroy(ctx, conf);
    configuration_free(conf->params);
    free(conf);
}

// Pass IPFIX data with supplemental structures into the storage plugin.
int
ipx_plugin_process(ipx_ctx_t *ctx, void *cfg, ipx_msg_t *msg)
{
    (void) ctx;
    struct conf_unirec *conf = (struct conf_unirec *) cfg;
    const bool split_enabled = conf->params->biflow_split;
    IPX_CTX_DEBUG(ctx, "Received a new message to process.");

    uint16_t msg_size = 0;
    const void *msg_data = NULL;

    ipx_msg_ipfix_t *ipfix = ipx_msg_base2ipfix(msg);
    const uint8_t *ipfix_raw_msg = ipx_msg_ipfix_get_packet(ipfix);
    translator_set_context(conf->trans, (const struct fds_ipfix_msg_hdr *) ipfix_raw_msg);

    const uint32_t rec_cnt = ipx_msg_ipfix_get_drec_cnt(ipfix);
    for (uint32_t i = 0; i < rec_cnt; i++) {
        // Get a pointer to the next record
        struct ipx_ipfix_record *ipfix_rec = ipx_msg_ipfix_get_drec(ipfix, i);
        bool rec_is_biflow = (ipfix_rec->rec.tmplt->flags & FDS_TEMPLATE_BIFLOW) != 0;
        bool biflow_split = split_enabled && rec_is_biflow;

        // Fill record (Note: in case of biflow split, forward fields only)
        uint16_t flags = biflow_split ? (FDS_DREC_BIFLOW_FWD | FDS_DREC_REVERSE_SKIP) : 0;
        msg_data = translator_translate(conf->trans, &ipfix_rec->rec, flags, &msg_size);
        if (!msg_data) {
            // Nothing to send
            continue;
        }

        IPX_CTX_DEBUG(ctx, "Send via TRAP IFC.");
        trap_ctx_send(conf->trap_ctx, 0, msg_data, msg_size);

        // Is it biflow and split is enabled? Send the reverse direction
        if (!biflow_split) {
            continue;
        }

        flags = FDS_DREC_BIFLOW_REV | FDS_DREC_REVERSE_SKIP;
        msg_data = translator_translate(conf->trans, &ipfix_rec->rec, flags, &msg_size);
        if (!msg_data) {
            // Nothing to send
            continue;
        }

        IPX_CTX_DEBUG(ctx, "Send via TRAP IFC.");
        trap_ctx_send(conf->trap_ctx, 0, msg_data, msg_size);
    }

    return 0;
}

