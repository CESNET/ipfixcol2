/**
 * \file src/plugins/intermediate/geo/geo.c
 * \author Adam Zvara <xzvara01@stud.fit.vutbr.cz>
 * \brief Configuration parser of GeoIP plugin (source file)
 * \date 2023
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
#include <unistd.h> // access()
#include <errno.h>
#include "config.h"

/*
 * <params>
 *  <path>...</path>
 * </params>
 */

/** XML nodes */
enum params_xml_nodes {
    GEO_PATH = 1,
};

/** Definition of the \<params\> node  */
static const struct fds_xml_args args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_ELEM(GEO_PATH, "path", FDS_OPTS_T_STRING, 0),
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
config_parser_root(ipx_ctx_t *ctx, fds_xml_ctx_t *root, struct geo_config *cfg)
{
    (void) ctx;
    size_t path_len;

    const struct fds_xml_cont *content;
    while (fds_xml_next(root, &content) != FDS_EOC) {
        switch (content->id) {
        case GEO_PATH:
            assert(content->type == FDS_OPTS_T_STRING);
            path_len = strlen(content->ptr_string);
            cfg->db_path = strndup(content->ptr_string, path_len);
            if (!cfg->db_path) {
                IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
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
 * \brief Check configuration parameters
 * \param[in] ctx Instance context
 * \param[in] cfg Parsed configuration
 * \return #IPX_OK on success (no fatal error)
 * \return #IPX_ERR_FORMAT in case of fatal failure
 */
static int
config_check(ipx_ctx_t *ctx, struct geo_config *cfg)
{
    if (!cfg->db_path) {
        IPX_CTX_ERROR(ctx, "Path to database is not set");
        return IPX_ERR_FORMAT;
    }

    // Check if database file exists and user has permission to read it
    int permissions = R_OK; // File exists and user has read permissions

    if (access(cfg->db_path, permissions)) {
        switch(errno) {
            case ENOENT:
                IPX_CTX_ERROR(ctx, "Could not find database file");
                break;
            case EACCES:
                IPX_CTX_ERROR(ctx, "Insufficient permissions on database file");
                break;
            default:
                IPX_CTX_ERROR(ctx, "Unexpected error occured (%s:%d)", __FILE__, __LINE__);
                break;
        }
        return IPX_ERR_FORMAT;
    }

    return IPX_OK;
}

struct geo_config *
config_parse(ipx_ctx_t *ctx, const char *params)
{
    struct geo_config *cfg = calloc(1, sizeof(*cfg));
    if (!cfg) {
        IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
        return NULL;
    }

    // Create an XML parser
    fds_xml_t *parser = fds_xml_create();
    if (!parser) {
        IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
        free(cfg);
        return NULL;
    }
    if (fds_xml_set_args(parser, args_params) != IPX_OK) {
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

    // Check validity of configuration
    if (config_check(ctx, cfg) != IPX_OK) {
        config_destroy(cfg);
        return NULL;
    }

    return cfg;
}

void
config_destroy(struct geo_config *cfg)
{
    free(cfg->db_path);
    free(cfg);
}