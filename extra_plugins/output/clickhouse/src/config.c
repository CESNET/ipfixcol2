/**
 * \file
 * \author Petr Szczurek <xszczu00@stud.fit.vut.cz>
 * \brief Parser of an XML configuration
 *
 * Copyright(c) 2023 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <limits.h>
#include "config.h"

/** XML nodes */
enum params_xml_nodes {
    NODE_CONN = 1,
    NODE_TABLE
};

/** Definition of the \<connection\> node  */
static const struct fds_xml_args args_connection[] = {
    // TODO
    FDS_OPTS_END
};

/** Definition of the \<table\> node  */
static const struct fds_xml_args args_table[] = {
    // TODO
    FDS_OPTS_END
};

/** Definition of the \<params\> node  */
static const struct fds_xml_args args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_NESTED(NODE_CONN, "connection", args_connection, FDS_OPTS_P_OPT),
    FDS_OPTS_NESTED(NODE_TABLE, "table", args_table, FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

static int
config_parser_conn(ipx_ctx_t *ctx, fds_xml_ctx_t *root, struct config *cfg)
{
    // TODO
    (void) ctx;
    (void) root;
    (void) cfg;

    return IPX_OK;
}

static int
config_parser_table(ipx_ctx_t *ctx, fds_xml_ctx_t *root, struct config *cfg)
{
    // TODO
    (void) ctx;
    (void) root;
    (void) cfg;

    return IPX_OK;
}


static int
config_parser_root(ipx_ctx_t *ctx, fds_xml_ctx_t *root, struct config *cfg)
{
    int rc;

    const struct fds_xml_cont *content;
    while (fds_xml_next(root, &content) != FDS_EOC) {
        switch (content->id) {
        case NODE_CONN:
            // Database connection parameters
            assert(content->type == FDS_OPTS_T_CONTEXT);
            if ((rc = config_parser_conn(ctx, content->ptr_ctx, cfg)) != IPX_OK) {
                return rc;
            }
            break;
        case NODE_TABLE:
            // Table parameters
            assert(content->type == FDS_OPTS_T_CONTEXT);
            if ((rc = config_parser_table(ctx, content->ptr_ctx, cfg)) != IPX_OK) {
                return rc;
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
config_default_set(struct config *cfg)
{
    // TODO
    (void) cfg;
}

struct config *
config_create(ipx_ctx_t *ctx, const char *params)
{
    struct config *cfg = calloc(1, sizeof(*cfg));
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
config_destroy(struct config *cfg)
{
    free(cfg);
}
