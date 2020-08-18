/**
 * \file src/plugins/output/dummy/config.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Example parser of an XML configuration (source file)
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
#include <limits.h>
#include "config.h"

/*
 * <params>
 *  <delay>...</delay>          <!-- in microseconds -->
 *  <failAfter>...</failAfter>  <!-- number of messages before -->
 *  <failType>...</failType>    <!-- return code of the module -->
 * </params>
 */

/** XML nodes */
enum params_xml_nodes {
    NODE_DELAY = 1,
    NODE_FAIL_AFTER,
    NODE_FAIL_TYPE
};

/** Definition of the \<params\> node  */
static const struct fds_xml_args args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_ELEM(NODE_DELAY, "delay", FDS_OPTS_T_UINT, 0),
    FDS_OPTS_ELEM(NODE_FAIL_AFTER, "failAfter", FDS_OPTS_T_UINT, 0),
    FDS_OPTS_ELEM(NODE_FAIL_TYPE, "failType", FDS_OPTS_T_STRING, 0),
    FDS_OPTS_END
};

/**
 * \brief Process \<params\> node
 * \param[in] ctx  Plugin context
 * \param[in] root XML context to process
 * \param[in] cfg  Parsed configuration
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT in case of failure
 */
static int
config_parser_root(ipx_ctx_t *ctx, fds_xml_ctx_t *root, struct instance_config *cfg)
{
    (void) ctx;

    const struct fds_xml_cont *content;
    while(fds_xml_next(root, &content) != FDS_EOC) {
        switch (content->id) {
        case NODE_DELAY:
            // Delay between messages [microseconds]
            assert(content->type == FDS_OPTS_T_UINT);
            cfg->sleep_time.tv_nsec = (content->val_uint % 1000000LL) * 1000LL;
            cfg->sleep_time.tv_sec  = content->val_uint / 1000000LL;
            break;
        case NODE_FAIL_AFTER:
            // Fail after generating specific number of messages
            assert(content->type == FDS_OPTS_T_UINT);
            cfg->fail_after = content->val_uint;
            break;
        case NODE_FAIL_TYPE:
            assert(content->type == FDS_OPTS_T_STRING);
            if (strcasecmp(content->ptr_string, "IPX_ERR_DENIED") == 0) {
                cfg->fail_type = IPX_ERR_DENIED;
            } else if (strcasecmp(content->ptr_string, "IPX_ERR_EOF") == 0) {
                cfg->fail_type = IPX_ERR_EOF;
            } else {
                IPX_CTX_ERROR(ctx, "Invalid failure type (expected: IPX_ERR_EOF/IPX_ERR_DENIED", '\0');
                return IPX_ERR_FORMAT;
            }
            break;
        default:
            // Internal error
            assert(false);
        }
    }

    return IPX_OK;
}

/**
 * \brief Set default parameters of the configuration
 * \param[in] cfg Configuration
 */
static void
config_default_set(struct instance_config *cfg)
{
    cfg->sleep_time.tv_sec = 0;
    cfg->sleep_time.tv_nsec = 100000000LL; // 100ms between messages
}

struct instance_config *
config_parse(ipx_ctx_t *ctx, const char *params)
{
    struct instance_config *cfg = calloc(1, sizeof(*cfg));
    if (!cfg) {
        IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
        return NULL;
    }

    // Set default parameters
    config_default_set(cfg);

    // Create an XML parser
    fds_xml_t *parser = fds_xml_create();
    if (!parser) {
        IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
        free(cfg);
        return NULL;
    }

    if (fds_xml_set_args(parser, args_params) != FDS_OK) {
        IPX_CTX_ERROR(ctx, "Failed to parse the description of an XML document!", '\0');
        fds_xml_destroy(parser);
        free(cfg);
        return NULL;
    }

    fds_xml_ctx_t *params_ctx = fds_xml_parse_mem(parser, params, true);
    if (params_ctx == NULL) {
        IPX_CTX_ERROR(ctx, "Failed to parse the configuration: %s", fds_xml_last_err(parser));
        fds_xml_destroy(parser);
        free(cfg);
        return NULL;
    }

    // Parse parameters
    int rc = config_parser_root(ctx, params_ctx, cfg);
    fds_xml_destroy(parser);
    if (rc != IPX_OK) {
        free(cfg);
        return NULL;
    }

    return cfg;
}

void
config_destroy(struct instance_config *cfg)
{
    free(cfg);
}
