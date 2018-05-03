/**
 * \file src/plugins/intermediate/anonymization/config.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Configuration parser of anonymization plugin (source file)
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
 * </params>
 */

/** XML nodes */
enum params_xml_nodes {
    ANON_TYPE = 1,
    ANON_KEY
};

/** Definition of the \<params\> node  */
static const struct fds_xml_args args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_ELEM(ANON_TYPE, "type", FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(ANON_KEY,  "key",  FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
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
config_parser_root(ipx_ctx_t *ctx, fds_xml_ctx_t *root, struct anon_config *cfg)
{
    (void) ctx;
    size_t key_len;

    const struct fds_xml_cont *content;
    while (fds_xml_next(root, &content) != FDS_EOC) {
        switch (content->id) {
        case ANON_TYPE:
            assert(content->type == FDS_OPTS_T_STRING);
            if (strcasecmp(content->ptr_string, "cryptopan") == 0) {
                cfg->mode = AN_CRYPTOPAN;
            } else if (strcasecmp(content->ptr_string, "truncation") == 0) {
                cfg->mode = AN_TRUNC;
            } else {
                IPX_CTX_ERROR(ctx, "Unrecognized <type> of anonymization technique.", '\0');
                return IPX_ERR_FORMAT;
            }
            break;
        case ANON_KEY:
            assert(content->type == FDS_OPTS_T_STRING);
            key_len = strlen(content->ptr_string);
            if (key_len < ANON_KEY_LEN) {
                IPX_CTX_ERROR(ctx, "Anonymization key is too short! Expected length is %d bytes.",
                    (int) ANON_KEY_LEN);
                return IPX_ERR_FORMAT;
            }

            if (key_len > ANON_KEY_LEN) {
                IPX_CTX_WARNING(ctx, "Anonymization key is longer that %d bytes. Extra bytes will "
                    "be ignored!", (int) ANON_KEY_LEN);
            }

            cfg->crypto_key = strndup(content->ptr_string, ANON_KEY_LEN);
            if (!cfg->crypto_key) {
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
config_check(ipx_ctx_t *ctx, struct anon_config *cfg)
{
    if (cfg->mode == AN_CRYPTOPAN && cfg->crypto_key == NULL) {
        IPX_CTX_WARNING(ctx, "Crypto-PAn key is not defined! Random will be generated!", '\0');
        char *key = malloc((ANON_KEY_LEN + 1) * sizeof(char));
        if (!key) {
            IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
            return IPX_ERR_FORMAT;
        }

        const char *key_src = "/dev/urandom";
        FILE *file = fopen(key_src, "rb");
        if (file == NULL || fread(key, ANON_KEY_LEN, 1, file) != 1) {
            IPX_CTX_ERROR(ctx, "Failed to get random key from '%s'!", key_src);
            free(key);
            if (file != NULL) {
                fclose(file);
            }
            return IPX_ERR_FORMAT;
        }
        fclose(file);

        key[ANON_KEY_LEN] = '\0';
        cfg->crypto_key = key;
    }

    if (cfg->mode == AN_TRUNC && cfg->crypto_key != NULL) {
        IPX_CTX_WARNING(ctx, "Selected technique ignores the given key.", '\0');
    }

    return IPX_OK;
}

struct anon_config *
config_parse(ipx_ctx_t *ctx, const char *params)
{
    struct anon_config *cfg = calloc(1, sizeof(*cfg));
    if (!cfg) {
        IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
        return NULL;
    }

    // Set default parameters
    cfg->crypto_key = NULL;

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
        free(cfg->crypto_key);
        free(cfg);
        return NULL;
    }

    // Check validity of configuration
    if (config_check(ctx, cfg) != IPX_OK) {
        config_destroy(cfg);
    }

    return cfg;
}

void
config_destroy(struct anon_config *cfg)
{
    free(cfg->crypto_key);
    free(cfg);
}
