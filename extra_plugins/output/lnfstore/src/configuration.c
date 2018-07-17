/**
 * \file configuration.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Configuration parser (source file)
 */

 /* Copyright (C) 2016-2018 CESNET, z.s.p.o.
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

#include <libfds.h>

#include "configuration.h"
#include "lnfstore.h"
#include "idx_manager.h"
#include "utils.h"

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define SUFFIX_MASK             "%Y%m%d%H%M%S"
#define LNF_FILE_PREFIX         "lnf."
#define BF_FILE_PREFIX          "bfi."
#define BF_DEFAULT_FP_PROB      0.01
#define BF_DEFAULT_ITEM_CNT_EST 100000

#define WINDOW_SIZE             300U

/*
 * <params>
 *  <storagePath>...</storagePath>
 *  <suffixMask>...</suffixMask>                   <!-- optional -->
 *  <identificatorField>...</identificatorField>   <!-- optional -->
 *  <compress>...</compress>                       <!-- optional -->
 *  <dumpInterval>
 *    <timeWindow>...</timeWindow>
 *    <align>...</align>
 *  </dumpInterval>
 *  <index>
 *    <enable>...</enable>
 *    <autosize>...</autosize>
 *    <estimatedItemCount>...</estimatedItemCount>
 *    <falsePositiveProbability>...</falsePositiveProbability>
 *  </index>
 * </params>
 */

/** XML nodes */
enum params_xml_nodes {
    NODE_STORAGE = 1,
    NODE_ID_FIELD,
    NODE_COMPRESS,
    NODE_DUMP,
    NODE_IDX,

    DUMP_WINDOW,
    DUMP_ALIGN,

    IDX_ENABLE,
    IDX_AUTOSIZE,
    IDX_COUNT,
    IDX_PROB
};


/** Definition of the \<dumInterval\> node  */
static const struct fds_xml_args args_dump[] = {
    FDS_OPTS_ELEM(DUMP_WINDOW,  "timeWindow",          FDS_OPTS_T_UINT, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(DUMP_ALIGN,   "align",               FDS_OPTS_T_BOOL, FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

/** Definition of the \<index\> node  */
static const struct fds_xml_args args_idx[] = {
    FDS_OPTS_ELEM(IDX_ENABLE,   "enable",              FDS_OPTS_T_BOOL,   FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(IDX_AUTOSIZE, "autosize",            FDS_OPTS_T_BOOL,   FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(IDX_COUNT,    "estimatedItemCount",  FDS_OPTS_T_UINT,   FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(IDX_PROB,     "falsePositiveProbability", FDS_OPTS_T_DOUBLE, FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

/** Definition of the \<params\> node  */
static const struct fds_xml_args args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_ELEM(NODE_STORAGE,  "storagePath",        FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(NODE_ID_FIELD, "identificatorField", FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(NODE_COMPRESS, "compress",           FDS_OPTS_T_BOOL,   FDS_OPTS_P_OPT),
    FDS_OPTS_NESTED(NODE_DUMP,   "dumpInterval",       args_dump,         FDS_OPTS_P_OPT),
    FDS_OPTS_NESTED(NODE_IDX,    "index",              args_idx,          FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

/**
 * \brief Check validity of a configuration
 * \param[in] cfg Configuration
 * \return If the configuration is valid, returns #IPX_OK.
 * \return Otherwise returns #IPX_ERR_FORMAT
 */
static int
configuration_validate(ipx_ctx_t *ctx, const struct conf_params *cfg)
{
    int ret_code = IPX_OK;

    if (!cfg->profiles.en && !cfg->files.path) {
        IPX_CTX_ERROR(ctx, "Storage path is not set.", '\0');
        ret_code = IPX_ERR_FORMAT;
    }

    if (!cfg->files.suffix) {
        IPX_CTX_ERROR(ctx, "File suffix is not set.", '\0');
        ret_code = IPX_ERR_FORMAT;
    }

    if (!cfg->file_lnf.prefix) {
        IPX_CTX_ERROR(ctx, "LNF file prefix is not set.", '\0');
        ret_code = IPX_ERR_FORMAT;
    }

    if (cfg->file_index.en) {
        if (!cfg->file_index.prefix) {
            IPX_CTX_ERROR(ctx, "Index file prefix is not set.", '\0');
            ret_code = IPX_ERR_FORMAT;
        }

        if (cfg->file_index.est_cnt == 0) {
            IPX_CTX_ERROR(ctx, "Estimated item count in Bloom Filter Index must be greater "
                "than 0.", '\0');
            ret_code = IPX_ERR_FORMAT;
        }

        if (cfg->file_index.fp_prob > FPP_MAX ||
            cfg->file_index.fp_prob < FPP_MIN) {
            IPX_CTX_ERROR(ctx, "Wrong false positive probability value. "
                "Use a value from %f to %f.", FPP_MIN, FPP_MAX);
            ret_code = IPX_ERR_FORMAT;
        }

        // Check output prefixes
        if (strcmp(cfg->file_index.prefix, cfg->file_lnf.prefix) == 0) {
            IPX_CTX_ERROR(ctx, "The same file prefix for LNF and Index file is not allowed");
            ret_code = IPX_ERR_FORMAT;
        }
    }

    if (cfg->window.size == 0) {
        IPX_CTX_ERROR(ctx, "Window size must be greater than 0.", '\0');
        ret_code = IPX_ERR_FORMAT;
    }

    return ret_code;
}

/**
 * \brief Set default configuration params
 * \param[in,out] cnf Configuration
 * \return On success returns #IPX_OK.
 * \return Otherwise returns #IPX_ERR_NOMEM, the content of the configuration is undefined and
 *   it MUST be destroyed by configuration_free()
 */
static int
configuration_set_defaults(ipx_ctx_t *ctx, struct conf_params *cnf)
{
    cnf->ctx = ctx;
    cnf->profiles.en = false; // Disabled by default

    // Dump interval
    cnf->window.align = true;
    cnf->window.size = WINDOW_SIZE;

    // Files (common)
    cnf->files.suffix = strdup(SUFFIX_MASK);
    if (!cnf->files.suffix) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        return 1;
    }

    // LNF file
    cnf->file_lnf.compress = false;
    cnf->file_lnf.prefix = strdup(LNF_FILE_PREFIX);
    if (!cnf->file_lnf.prefix) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        return 1;
    }

    // Index file
    cnf->file_index.en = false;
    cnf->file_index.autosize = true;
    cnf->file_index.est_cnt = BF_DEFAULT_ITEM_CNT_EST;
    cnf->file_index.fp_prob = BF_DEFAULT_FP_PROB;
    cnf->file_index.prefix = strdup(BF_FILE_PREFIX);
    if (!cnf->file_index.prefix) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        return 1;
    }

    return 0;
}

/**
 * \brief Auxiliary function for parsing <dumpInterval> options
 * \param[in] ctx  Instance context (just for log)
 * \param[in] dump XML context to process
 * \param[in] cnf  Configuration
 * \return On success returns #IPX_OK. Otherwise returns #IPX_ERR_DENIED.
 */
static int
configuration_parse_dump(ipx_ctx_t *ctx, fds_xml_ctx_t *dump, struct conf_params *cnf)
{
    const struct fds_xml_cont *content;
    while(fds_xml_next(dump, &content) != FDS_EOC) {
        switch (content->id) {
        case DUMP_WINDOW:
            assert(content->type == FDS_OPTS_T_UINT);
            if (content->val_uint > UINT32_MAX) {
                IPX_CTX_ERROR(ctx, "Window size is too long!", '\0');
                return IPX_ERR_DENIED;
            }

            cnf->window.size = (uint32_t) content->val_uint;
            break;
        case DUMP_ALIGN:
            assert(content->type == FDS_OPTS_T_BOOL);
            cnf->window.align = content->val_bool;
            break;
        default:
            assert(false);
        }
    }

    return IPX_OK;
}

/**
 * \brief Auxiliary function for parsing <index> options
 * \param[in] ctx  Instance context (just for log)
 * \param[in] idx  XML context to process
 * \param[in] cnf  Configuration
 * \return On success returns #IPX_OK. Otherwise returns #IPX_ERR_DENIED.
 */
static int
configuration_parse_idx(ipx_ctx_t *ctx, fds_xml_ctx_t *idx, struct conf_params *cnf)
{
    (void) ctx;

    const struct fds_xml_cont *content;
    while(fds_xml_next(idx, &content) != FDS_EOC) {
        switch (content->id) {
        case IDX_ENABLE:
            assert(content->type == FDS_OPTS_T_BOOL);
            cnf->file_index.en = content->val_bool;
            break;
        case IDX_AUTOSIZE:
            assert(content->type == FDS_OPTS_T_BOOL);
            cnf->file_index.autosize = content->val_bool;
            break;
        case IDX_COUNT:
            assert(content->type == FDS_OPTS_T_UINT);
            cnf->file_index.est_cnt = content->val_uint;
            break;
        case IDX_PROB:
            assert(content->type == FDS_OPTS_T_DOUBLE);
            cnf->file_index.fp_prob = content->val_double; // checked later in validator
            break;
        default:
            assert(false);
        }
    }

    return IPX_OK;
}

/**
 * \brief Auxiliary function for parsing <params> options
 * \param[in] ctx  Instance context (just for log)
 * \param[in] root XML context to process
 * \param[in] cnf  Configuration
 * \return On success returns #IPX_OK. Otherwise returns #IPX_ERR_DENIED.
 */
static int
configuration_parse_root(ipx_ctx_t *ctx, fds_xml_ctx_t *root, struct conf_params *cnf)
{
    const struct fds_xml_cont *content;
    while(fds_xml_next(root, &content) != FDS_EOC) {
        switch (content->id) {
        case NODE_STORAGE:
            assert(content->type == FDS_OPTS_T_STRING);
            cnf->files.path = utils_path_preprocessor(content->ptr_string);
            if (!cnf->files.path) {
                const char *err_str;
                ipx_strerror(errno, err_str);
                IPX_CTX_ERROR(ctx, "Failed to process the <storagePath> expression: %s", err_str);
                return IPX_ERR_DENIED;
            }
            break;
        case NODE_ID_FIELD:
            assert(content->type == FDS_OPTS_T_STRING);
            free(cnf->file_lnf.ident);
            cnf->file_lnf.ident = strdup(content->ptr_string);
            break;
        case NODE_COMPRESS:
            assert(content->type == FDS_OPTS_T_BOOL);
            cnf->file_lnf.compress = content->val_bool;
            break;
        case NODE_DUMP:
            assert(content->type == FDS_OPTS_T_CONTEXT);
            if (configuration_parse_dump(ctx, content->ptr_ctx, cnf) != IPX_OK) {
                return IPX_ERR_DENIED;
            }
            break;
        case NODE_IDX:
            assert(content->type == FDS_OPTS_T_CONTEXT);
            if (configuration_parse_idx(ctx, content->ptr_ctx, cnf) != IPX_OK) {
                return IPX_ERR_DENIED;
            }
            break;
        default:
            // Internal error
            assert(false);
        }
    }

    return IPX_OK;
}

struct conf_params *
configuration_parse(ipx_ctx_t *ctx, const char *params)
{
    if (!params) {
        return NULL;
    }

    struct conf_params *cnf = calloc(1, sizeof(struct conf_params));
    if (!cnf) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        return NULL;
    }

    if (configuration_set_defaults(ctx, cnf) != IPX_OK) {
        // Failed
        configuration_free(cnf);
        return NULL;
    }

    // Try to parse plugin configuration
    fds_xml_t *parser = fds_xml_create();
    if (!parser) {
        IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
        configuration_free(cnf);
        return NULL;
    }

    if (fds_xml_set_args(parser, args_params) != FDS_OK) {
        IPX_CTX_ERROR(ctx, "Failed to parse the description of an XML document!", '\0');
        fds_xml_destroy(parser);
        configuration_free(cnf);
        return NULL;
    }

    fds_xml_ctx_t *params_ctx = fds_xml_parse_mem(parser, params, true);
    if (params_ctx == NULL) {
        IPX_CTX_ERROR(ctx, "Failed to parse the configuration: %s", fds_xml_last_err(parser));
        fds_xml_destroy(parser);
        configuration_free(cnf);
        return NULL;
    }

    // Parse parameters
    int rc = configuration_parse_root(ctx, params_ctx, cnf);
    fds_xml_destroy(parser);
    if (rc != IPX_OK) {
        configuration_free(cnf);
        return NULL;
    }

    // Check combinations
    if (configuration_validate(ctx, cnf) != IPX_OK) {
        // Failed!
        configuration_free(cnf);
        return NULL;
    }

    return cnf;
}

void
configuration_free(struct conf_params *config)
{
    if (!config) {
        return;
    }

    free(config->files.path);

    if (config->files.suffix) {
        free(config->files.suffix);
    }

    if (config->file_lnf.prefix) {
        free(config->file_lnf.prefix);
    }

    if (config->file_lnf.ident) {
        free(config->file_lnf.ident);
    }

    if (config->file_index.prefix) {
        free(config->file_index.prefix);
    }

    free(config);
}
