/**
 * \file src/plugins/intermediate/filter/config.c
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief The filter plugin config
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

#include "config.h"

#include <string.h>
#include <stdlib.h>

/*
 * <params>
 *   <expr>...</expr>
 * </params>
 */

enum params_xml_nodes {
    EXTENSION_EXPR = 1,
    EXTENSION_ID = 2,
    EXTENSION_VALUES = 3,
    EXTENSION_VALUE = 4,
    EXTENSION_IDS = 5
};

static const struct fds_xml_args values_params[] = {
    FDS_OPTS_ELEM(EXTENSION_EXPR, "expr", FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(EXTENSION_VALUE, "value", FDS_OPTS_T_STRING, 0),
    FDS_OPTS_END
};

static const struct fds_xml_args ids_params[] = {
    FDS_OPTS_ELEM(EXTENSION_ID, "id", FDS_OPTS_T_STRING, 0),    
    FDS_OPTS_NESTED(EXTENSION_VALUES, "values", values_params, FDS_OPTS_P_OPT | FDS_OPTS_P_MULTI),
    FDS_OPTS_END
};

static const struct fds_xml_args args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_NESTED(EXTENSION_IDS, "ids", ids_params, FDS_OPTS_P_OPT | FDS_OPTS_P_MULTI),
    FDS_OPTS_END
};

int config_parse_values(ipx_ctx_t *ctx, const struct fds_xml_cont *content, config_ids_t* id) {
    const struct fds_xml_cont *value_content;
    while (fds_xml_next(content->ptr_ctx, &value_content) == FDS_OK) {
        switch (value_content->id) {
            case EXTENSION_VALUE:
                assert(value_content->type == FDS_OPTS_T_STRING);
                if (strlen(value_content->ptr_string) == 0) {
                    IPX_CTX_ERROR(ctx, "Extension value is empty!");
                    return -1;
                }
                id->values[id->values_count].value = strdup(value_content->ptr_string);
                if (!id->values[id->values_count].value) {
                    IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
                    return -1;
                }
                break;
            case EXTENSION_EXPR:
                assert(value_content->type == FDS_OPTS_T_STRING);
                if (strlen(value_content->ptr_string) == 0) {
                    IPX_CTX_ERROR(ctx, "Filter expression is empty!");
                    return -1;
                }
                id->values[id->values_count].expr = strdup(value_content->ptr_string);
                if (!id->values[id->values_count].expr) {
                    IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
                    return -1;
                }
                break;
            default:
                break;
        }
    }
    return 0;
}

int config_parse_ids(ipx_ctx_t *ctx, const struct fds_xml_cont *content, config_ids_t* id) {
    const struct fds_xml_cont *id_content;
    while (fds_xml_next(content->ptr_ctx, &id_content) == FDS_OK) {
        switch (id_content->id) {
            case EXTENSION_ID:
                // Ignored in this plugin
                assert(id_content->type == FDS_OPTS_T_STRING);
                if (strlen(id_content->ptr_string) == 0) {
                    IPX_CTX_ERROR(ctx, "Extension ID is empty!");
                    return -1;
                }
                id->name = strdup(id_content->ptr_string);
                if (!id->name) {
                    IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
                    return -1;
                }
                break;
            case EXTENSION_VALUES:
                if( id->values_count >= CONFIG_VALUSE_MAX ) {
                    IPX_CTX_ERROR(ctx, "Maximum number of extension values per id exceeded (%d)!", CONFIG_VALUSE_MAX);
                    return -1;
                }
                config_parse_values(ctx, id_content, id);
                id->values_count++;
                break;
            default:
                break;
        }
    }
    return 0;
}

struct config *
config_parse(ipx_ctx_t *ctx, const char *params)
{
    struct config *cfg = NULL;
    fds_xml_t *parser = NULL;

    cfg = calloc(1, sizeof(struct config));
    if (!cfg) {
        IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
        goto error;
    }

    parser = fds_xml_create();
    if (!parser) {
        IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
        goto error;
    }

    if (fds_xml_set_args(parser, args_params) != FDS_OK) {
        IPX_CTX_ERROR(ctx, "Failed to parse the description of an XML document!");
        goto error;
    }

    fds_xml_ctx_t *params_ctx = fds_xml_parse_mem(parser, params, true);
    if (params_ctx == NULL) {
        IPX_CTX_ERROR(ctx, "Failed to parse the configuration: %s", fds_xml_last_err(parser));
        goto error;
    }

    const struct fds_xml_cont *content;
    while (fds_xml_next(params_ctx, &content) == FDS_OK) {
        switch (content->id) {
            case EXTENSION_IDS:
            {
                if( cfg->ids_count >= CONFIG_IDS_MAX ) {
                    IPX_CTX_ERROR(ctx, "Maximum number of extension uniq ids exceeded (%d)!", CONFIG_IDS_MAX);
                    goto error;
                }
                config_ids_t* id = &cfg->ids[cfg->ids_count];
                id->name = NULL;                
                if( config_parse_ids(ctx, content, id) != 0 ) {
                    goto error;
                }
                cfg->ids_count++;
                break;
            }
            default:
                break;
        }
    }
    fds_xml_destroy(parser);
    return cfg;

error:
    fds_xml_destroy(parser);
    config_destroy(cfg);
    return NULL;
}

void
config_destroy(struct config *cfg)
{
    if (cfg == NULL) {
        return;
    }
    for( int i = 0; i < cfg->ids_count; i++) {
        printf("Free filter %s\n", cfg->ids[i].name);
        for( int v =0; v < cfg->ids[i].values_count; v++) {
            if( cfg->ids[i].values[v].expr ) {
                free(cfg->ids[i].values[v].expr);
                cfg->ids[i].values[v].expr = NULL;
            }
            if( cfg->ids[i].values[v].value ) {
                free(cfg->ids[i].values[v].value);
                cfg->ids[i].values[v].value = NULL;
            }
            if( cfg->ids[i].values[v].filter ) {
                fds_ipfix_filter_destroy(cfg->ids[i].values[v].filter);
                cfg->ids[i].values[v].filter = NULL;
            }
        }
        if( cfg->ids[i].name ) {
            free(cfg->ids[i].name);
            cfg->ids[i].name = NULL;
        }
    }
    free(cfg);
}