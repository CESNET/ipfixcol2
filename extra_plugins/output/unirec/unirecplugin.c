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

#define _GNU_SOURCE
#include <ipfixcol2.h>
#include <libtrap/trap.h>
#include <stdio.h>
#include <pthread.h>
#include <unirec/unirec.h>

#include "unirecplugin.h"
#include "fields.h"

static pthread_mutex_t urp_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * UniRec plugin reference counter
 *
 * This counter represents number of created instances (threads) of this plugin.
 * It must be incremented in init() and decremented in destroy().
 */
static uint8_t urp_refcount = 0;

/** Plugin description */
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin type
    .type = IPX_PT_OUTPUT,
    // Plugin identification name
    .name = "unirec",
    // Brief description of plugin
    .dsc = "Output plugin that sends flow records in UniRec format via TRAP communication interface (into NEMEA modules).",
    // Configuration flags (reserved for future use)
    .flags = 0,
    // Plugin version string (like "1.2.3")
    .version = "2.0.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    .ipx_min = "2.0.0"
};

static char *
clean_define_urtempl(const char *ut)
{
    char *res = strdup(ut);
    if (res == NULL) {
        return NULL;
    }

    size_t i, t;
    size_t len = strlen(ut);
    for (i = 0, t = 0; i < len; i++, t++) {
        if (res[i] == '?') {
            i++;
        }
        if (i > t) {
            res[t] = res[i];
        }
    }
    res[t] = 0;
    return res;
}

// Storage plugin initialization function.
int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params)
{
    IPX_CTX_INFO(ctx, "UniRec plugin initialization.");
    // Process XML configuration
    struct conf_params *parsed_params = configuration_parse(ctx, params);
    if (!parsed_params) {
        IPX_CTX_ERROR(ctx, "Failed to parse the plugin configuration.");
        goto error;
    }

    // Create main plugin structure
    struct conf_unirec *conf = calloc(1, sizeof(*conf));
    if (!conf) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        goto error;
    }

    conf->params = parsed_params;

    /* Load IPFIX2UniRec mapping file */
    IPX_CTX_INFO(ctx, "Load IPFIX to UniRec mapping file");
    unirecField_t *p;
    uint32_t urfieldcount, ipfixfieldcount;
    unirecField_t *map = load_IPFIX2UR_mapping(ctx, &urfieldcount, &ipfixfieldcount);
    if (map == NULL) {
        IPX_CTX_ERROR(ctx, "Failed to load IPFIX to UniRec mapping file (%s:%d)", __FILE__, __LINE__);
        goto error;
    }

    if (pthread_mutex_lock(&urp_mutex) == -1) {
        goto error;
    }

    for (p = map; p != NULL; p = p->next) {
        IPX_CTX_INFO(ctx, "Defining %s %s", p->unirec_type_str, p->name);
        ur_define_field(p->name, p->unirec_type);
    }

    if (pthread_mutex_unlock(&urp_mutex) == -1) {
        goto error;
    }

    /* Initialize TRAP Ctx */
    char *ifc_spec = configuration_create_ifcspec(ctx, parsed_params);
    if (ifc_spec == NULL) {
        goto error;
    }

    IPX_CTX_INFO(ctx, "Initialization of TRAP with IFCSPEC: '%s'.", ifc_spec);
    conf->tctx = trap_ctx_init3("IPFIXcol2-UniRec", "UniRec output plugin for IPFIXcol2.",
                                0, 1, ifc_spec, NULL /* TODO replace with some uniq name of service IFC */);

    free(ifc_spec);
    ifc_spec = NULL;

    if (conf->tctx == NULL) {
        IPX_CTX_ERROR(ctx, "Failed to initialize TRAP (%s:%d)", __FILE__, __LINE__);
        goto error;
    }
    if (trap_ctx_get_last_error(conf->tctx) != TRAP_E_OK) {
        IPX_CTX_ERROR(ctx, "Failed to initialize TRAP: %s (%s:%d)",
                      trap_ctx_get_last_error_msg(conf->tctx), __FILE__, __LINE__);
        goto error;
    }

    /* Clean UniRec template from '?' (optional fields) */
    char *cleaned_urtemplate = clean_define_urtempl(parsed_params->unirec_format);
    if (cleaned_urtemplate == NULL) {
        IPX_CTX_ERROR(ctx, "Could not allocate memory (%s:%d).", __FILE__, __LINE__);
        goto error;
    }
    IPX_CTX_INFO(ctx, "Cleaned UniRec template: '%s'.", cleaned_urtemplate);

    /* Allocate UniRec template */
    IPX_CTX_INFO(ctx, "Initialization of UniRec template.", ifc_spec);
    char *errstring = NULL;
    conf->urtmpl = ur_ctx_create_output_template(conf->tctx, 0, cleaned_urtemplate, &errstring);
    if (conf->urtmpl == NULL) {
        IPX_CTX_ERROR(ctx, "Failed to create UniRec template '%s' (%s:%d)", cleaned_urtemplate, __FILE__, __LINE__);
        goto error;
    }
    free(cleaned_urtemplate);
    cleaned_urtemplate = NULL;

    conf->translator = translator_init(ctx, map, ipfixfieldcount);
    if (conf->translator == NULL) {
        IPX_CTX_ERROR(ctx, "Failed to initialize a record translator.");
        goto error;
    }

    /* map is no longer needed, there is no copy, it can be deallocated */
    free_IPFIX2UR_map(map);
    map = NULL;

    char *urtmpl_str = ur_template_string_delimiter(conf->urtmpl, ',');
    IPX_CTX_INFO(ctx, "Using the following created UniRec template: \"%s\"", urtmpl_str);
    free(urtmpl_str);

    if (translator_init_urtemplate(conf->translator, conf->urtmpl, parsed_params->unirec_format) != 0) {
        IPX_CTX_ERROR(ctx, "Could not allocate memory (%s:%d)", __FILE__, __LINE__);
        goto error;
    }

    for (size_t i = 0; i < conf->urtmpl->count; ++i) {
        IPX_CTX_INFO(ctx, "\t%s\t%s", ur_get_name(conf->urtmpl->ids[i]), conf->translator->req_fields[i] ? "required" : "optional");
    }

    conf->ur_message = ur_create_record(conf->urtmpl, UR_MAX_SIZE);

    if (conf->ur_message == NULL) {
        IPX_CTX_ERROR(ctx, "Failed to allocate an UniRec record message.");
        goto error;
    }

    if (pthread_mutex_lock(&urp_mutex) == -1) {
        IPX_CTX_ERROR(ctx, "Could not lock (%s:%d)", __FILE__, __LINE__);
        goto error;
    }
    /* increase number of UniRec plugin references */
    urp_refcount++;
    if (pthread_mutex_unlock(&urp_mutex) == -1) {
        IPX_CTX_ERROR(ctx, "Could not unlock (%s:%d)", __FILE__, __LINE__);
        goto error;
    }

    // Save the configuration
    ipx_ctx_private_set(ctx, conf);

    IPX_CTX_INFO(ctx, "Plugin is ready.");
    return IPX_OK;

error:
    configuration_free(parsed_params);
    ur_finalize();
    free(ifc_spec);
    trap_ctx_finalize(&conf->tctx);
    free(cleaned_urtemplate);
    ipx_plugin_destroy(ctx, conf);
    free_IPFIX2UR_map(map);
    if (conf != NULL) {
        free(conf->ur_message);
        free(conf);
    }
    return IPX_ERR_DENIED;
}

// Pass IPFIX data with supplemental structures into the storage plugin.
int
ipx_plugin_process(ipx_ctx_t *ctx, void *cfg, ipx_msg_t *msg)
{
    (void) ctx;
    struct conf_unirec *conf = (struct conf_unirec *) cfg;

    IPX_CTX_INFO(ctx, "UniRec plugin process IPFIX message.");

    ipx_msg_ipfix_t *ipfix = ipx_msg_base2ipfix(msg);
    const uint32_t rec_cnt = ipx_msg_ipfix_get_drec_cnt(ipfix);
    void *message = conf->ur_message;
    for (uint32_t i = 0; i < rec_cnt; i++) {
        // Get a pointer to the next record
        struct ipx_ipfix_record *ipfix_rec = ipx_msg_ipfix_get_drec(ipfix, i);
        bool biflow = (ipfix_rec->rec.tmplt->flags & FDS_TEMPLATE_BIFLOW) != 0;

        // Fill record
        uint16_t flags = biflow ? FDS_DREC_BIFLOW_FWD : 0; // In case of biflow, forward fields only
        if (translator_translate(conf->translator, conf, &ipfix_rec->rec, flags) <= 0) {
            // Nothing to store
            continue;
        }
        IPX_CTX_INFO(ctx, "Send via TRAP IFC.");
        trap_ctx_send(conf->tctx, 0, message, ur_rec_size(conf->translator->urtmpl, message));

        // Is it biflow? Store the reverse direction
        if (!biflow) {
            continue;
        }

        flags = FDS_DREC_BIFLOW_REV;
        if (translator_translate(conf->translator, conf, &ipfix_rec->rec, flags) <= 0) {
            // Nothing to store
            continue;
        }

        IPX_CTX_INFO(ctx, "Send via TRAP IFC.");
        trap_ctx_send(conf->tctx, 0, message, ur_rec_size(conf->translator->urtmpl, message));
        break;
    }

    return 0;
}

// Storage plugin "destructor"
void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg)
{
    (void) ctx;
    struct conf_unirec *conf = (struct conf_unirec *) cfg;
    IPX_CTX_INFO(ctx, "UniRec plugin finalization.");

    if (pthread_mutex_lock(&urp_mutex) == -1) {
        IPX_CTX_ERROR(ctx, "Could not lock. (%s:%d)", __FILE__, __LINE__);
    }

    if (--urp_refcount == 0) {
        ur_finalize();
    }

    if (pthread_mutex_unlock(&urp_mutex) == -1) {
        IPX_CTX_ERROR(ctx, "Could not unlock. (%s:%d)", __FILE__, __LINE__);
    }

    if (conf != NULL) {
        trap_ctx_terminate(conf->tctx);
        // Destroy a translator and a record
        translator_destroy(conf->translator);

        // Destroy parsed XML configuration
        configuration_free(conf->params);

        trap_ctx_finalize(&conf->tctx);

        ur_free_template(conf->urtmpl);

        free(conf->ur_message);

        // Destroy instance structure
        free(conf);
    }
}

