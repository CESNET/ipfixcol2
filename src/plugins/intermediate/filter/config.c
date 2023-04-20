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
    FILTER_EXPR = 1,
};

static const struct fds_xml_args args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_ELEM(FILTER_EXPR, "expr", FDS_OPTS_T_STRING, 0),
    FDS_OPTS_END
};

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
        case FILTER_EXPR:
            assert(content->type == FDS_OPTS_T_STRING);
            if (strlen(content->ptr_string) == 0) {
                IPX_CTX_ERROR(ctx, "Filter expression is empty!");
                goto error;
            }
            cfg->expr = strdup(content->ptr_string);
            if (!cfg->expr) {
                IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
                goto error;
            }
            break;
        }
    }

    fds_xml_destroy(parser);
    return cfg;

error:
    fds_xml_destroy(parser);
    free(cfg);
    return NULL;
}

void
config_destroy(struct config *cfg)
{
    free(cfg->expr);
    free(cfg);
}