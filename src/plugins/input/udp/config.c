/**
 * \file src/plugins/input/udp/config.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Configuration parser of UDP input plugin (source file)
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
#include <string.h>
#include <inttypes.h>
#include "config.h"

/** Minimal connection timeout                                                                   */
#define CONN_TIMEOUT_MIN (10)
/** Default connection timeout                                                                   */
#define CONN_TIMEOUT_DEF (600)
/** Default Template Lifetime                                                                    */
#define LIFETIME_DATA_DEF (1800)
/** Default Options Template Lifetime                                                            */
#define LIFETIME_OPTS_DEF (1800)

/*
 * <params>
 *  <localPort>...</localPort>                    <!-- mandatory                 -->
 *  <localIPAddress>...</localIPAddress>          <!-- mandatory, multiple times -->
 *  <templateLifeTime>...</templateLifeTime>      <!-- optional                  -->
 *  <optionsTemplateLifeTime>...</optionsTemplateLifeTime> <!-- optional         -->
 *  <connectionTimeout>...</connectionTimeout>    <!-- optional                  -->
 * </params>
 */

/** XML nodes */
enum params_xml_nodes {
    NODE_PORT = 1,
    NODE_IPADDR,
    NODE_LT_DATA,
    NODE_LT_OPTS,
    NODE_TIMEOUT
};

/** Definition of the \<params\> node  */
static const struct fds_xml_args args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_ELEM(NODE_PORT,    "localPort",               FDS_OPTS_T_UINT,   0),
    FDS_OPTS_ELEM(NODE_IPADDR,  "localIPAddress",          FDS_OPTS_T_STRING, FDS_OPTS_P_MULTI),
    FDS_OPTS_ELEM(NODE_LT_DATA, "templateLifeTime",        FDS_OPTS_T_UINT,   FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(NODE_LT_OPTS, "optionsTemplateLifeTime", FDS_OPTS_T_UINT,   FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(NODE_TIMEOUT, "connectionTimeout",       FDS_OPTS_T_UINT,   FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

/**
 * \brief Add a local IP address
 *
 * \note An empty address is ignored and success is returned!
 * \param[in] ctx  Instance context
 * \param[in] cfg  Configuration
 * \param[in] addr IPv4/IPv6 address to add
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT if the address is malformed
 */
int
config_add_addr(ipx_ctx_t *ctx, struct udp_config *cfg, const char *addr)
{
    struct udp_ipaddr_rec rec;

    if (strlen(addr) == 0) {
        return IPX_OK;
    }

    // Try to convert IP address
    if (inet_pton(AF_INET, addr, &rec.ipv4) == 1) {
        rec.ip_ver = AF_INET;
    } else if (inet_pton(AF_INET6, addr, &rec.ipv6) == 1) {
        rec.ip_ver = AF_INET6;
    } else {
        IPX_CTX_ERROR(ctx, "'%s' is not a valid IPv4/IPv6 address!", addr);
        return IPX_ERR_FORMAT;
    }

    // Add the record
    size_t alloc_size = (cfg->local_addrs.cnt + 1) * sizeof(struct udp_ipaddr_rec);
    struct udp_ipaddr_rec *new_addrs = realloc(cfg->local_addrs.addrs, alloc_size);
    if (!new_addrs) {
        IPX_CTX_ERROR(ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        return IPX_ERR_FORMAT;
    }

    // Store the record
    new_addrs[cfg->local_addrs.cnt] = rec;
    cfg->local_addrs.cnt++;
    cfg->local_addrs.addrs = new_addrs;
    return IPX_OK;
}

/**
 * \brief Process \<params\> node
 * \param[in] ctx  Plugin context
 * \param[in] root XML context to process
 * \param[in] cfg  Parsed configuration
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT in case of failure
 */
static int
config_parser_root(ipx_ctx_t *ctx, fds_xml_ctx_t *root, struct udp_config *cfg)
{
    const struct fds_xml_cont *content;
    while (fds_xml_next(root, &content) != FDS_EOC) {
        switch (content->id) {
        case NODE_PORT:
            // Local port
            assert(content->type == FDS_OPTS_T_UINT);
            if (content->val_uint > UINT16_MAX) {
                IPX_CTX_ERROR(ctx, "Local port value must be between 0..%" PRIu16, UINT16_MAX);
                return IPX_ERR_FORMAT;
            }
            cfg->local_port = (uint16_t) content->val_uint;
            break;
        case NODE_IPADDR:
            // Local IP address
            assert(content->type == FDS_OPTS_T_STRING);
            if (config_add_addr(ctx, cfg, content->ptr_string) != IPX_OK) {
                return IPX_ERR_FORMAT;
            }
            break;
        case NODE_LT_DATA:
            // Template Lifetime
            assert(content->type == FDS_OPTS_T_UINT);
            if (content->val_uint > UINT16_MAX) {
                IPX_CTX_ERROR(ctx, "Template Lifetime must be between 0..%" PRIu16, UINT16_MAX);
                return IPX_ERR_FORMAT;
            }
            cfg->lifetime_data = (uint16_t) content->val_uint;
            break;
        case NODE_LT_OPTS:
            // Options Template Lifetime
            assert(content->type == FDS_OPTS_T_UINT);
            if (content->val_uint > UINT16_MAX) {
                IPX_CTX_ERROR(ctx, "Options Template Lifetime must be between 0..%" PRIu16,
                    UINT16_MAX);
                return IPX_ERR_FORMAT;
            }
            cfg->lifetime_opts = (uint16_t) content->val_uint;
            break;
        case NODE_TIMEOUT:
            // Connection timeout
            assert(content->type == FDS_OPTS_T_UINT);
            if (content->val_uint < CONN_TIMEOUT_MIN || content->val_uint > UINT16_MAX) {
                IPX_CTX_ERROR(ctx, "Connection timeout must be between %" PRIu16 "..%" PRIu16,
                    (uint16_t) CONN_TIMEOUT_MIN, UINT16_MAX);
                return IPX_ERR_FORMAT;
            }
            cfg->timeout_conn = (uint16_t) content->val_uint;
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
config_default_set(struct udp_config *cfg)
{
    cfg->local_port = 4739; // Default port
    cfg->local_addrs.cnt = 0;
    cfg->timeout_conn = CONN_TIMEOUT_DEF;
    cfg->lifetime_data = LIFETIME_DATA_DEF;
    cfg->lifetime_opts = LIFETIME_OPTS_DEF;
}

struct udp_config *
config_parse(ipx_ctx_t *ctx, const char *params)
{
    struct udp_config *cfg = calloc(1, sizeof(*cfg));
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
        config_destroy(cfg);
        return NULL;
    }

    if (fds_xml_set_args(parser, args_params) != IPX_OK) {
        IPX_CTX_ERROR(ctx, "Failed to parse the description of an XML document!", '\0');
        fds_xml_destroy(parser);
        config_destroy(cfg);
        return NULL;
    }

    fds_xml_ctx_t *params_ctx = fds_xml_parse_mem(parser, params, true);
    if (params_ctx == NULL) {
        IPX_CTX_ERROR(ctx, "Failed to parse the configuration: %s", fds_xml_last_err(parser));
        fds_xml_destroy(parser);
        config_destroy(cfg);
        return NULL;
    }

    // Parse parameters
    int rc = config_parser_root(ctx, params_ctx, cfg);
    fds_xml_destroy(parser);
    if (rc != IPX_OK) {
        config_destroy(cfg);
        return NULL;
    }

    return cfg;
}

void
config_destroy(struct udp_config *cfg)
{
    free(cfg->local_addrs.addrs);
    free(cfg);
}
