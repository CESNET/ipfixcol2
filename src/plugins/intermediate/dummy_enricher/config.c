/**
 * \file src/plugins/intermediate/asn/asn.c
 * \author Adam Zvara <xzvara01@stud.fit.vutbr.cz>
 * \brief Configuration parser of ASN plugin (source file)
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
 *  <field>
      <pen>0</pen>
      <id>1234</id>
      <type>string</type>
      <value>"ABCD"</value>
      <times>1</times>
    </field>
 * </params>
 */

// Strings used for parsing out type from XML file
#define STRING_TYPE "string"
#define INT_TYPE "integer"
#define DOUBLE_TYPE "double"

/** XML nodes */
enum params_xml_nodes {
    FIELD,
    PEN,
    ID,
    TYPE,
    VALUE,
    TIMES
};

// New fields
static const struct fds_xml_args fields_schema[] = {
    FDS_OPTS_ELEM(PEN,   "pen",   FDS_OPTS_T_UINT,   0),
    FDS_OPTS_ELEM(ID,    "id",    FDS_OPTS_T_UINT,   0),
    FDS_OPTS_ELEM(TYPE,  "type",  FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(VALUE, "value", FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(TIMES, "times", FDS_OPTS_T_UINT,   FDS_OPTS_P_OPT),
    FDS_OPTS_END
};


/** Definition of the \<params\> node  */
static const struct fds_xml_args args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_NESTED(FIELD, "field", fields_schema, FDS_OPTS_P_OPT | FDS_OPTS_P_MULTI),
    FDS_OPTS_END
};

static int
config_parser_fields(ipx_ctx_t *ctx, fds_xml_ctx_t *fields_ctx, struct dummy_config *cfg)
{
    const struct fds_xml_cont *fields;
    static size_t field_idx = 0;
    cfg->fields[field_idx].times = 1;

    while (fds_xml_next(fields_ctx, &fields) != FDS_EOC) {
        switch (fields->id) {
        case PEN:
            cfg->fields[field_idx].pen = fields->val_uint;
            break;

        case ID:
            cfg->fields[field_idx].id = fields->val_uint;
            break;

        case TYPE:
            if (strcmp(fields->ptr_string, INT_TYPE) == 0) {
                cfg->fields[field_idx].type = DUMMY_FIELD_INT;
            } else if (strcmp(fields->ptr_string, STRING_TYPE) == 0) {
                cfg->fields[field_idx].type = DUMMY_FIELD_STRING;
            } else if (strcmp(fields->ptr_string, DOUBLE_TYPE) == 0) {
                cfg->fields[field_idx].type = DUMMY_FIELD_DOUBLE;
            } else {
                IPX_CTX_ERROR(ctx, "Unknown type of field \"type\"");
                return IPX_ERR_FORMAT;
            }
            break;

        case VALUE:
            if (cfg->fields[field_idx].type == DUMMY_FIELD_INT) {
                int int_value = strtol(fields->ptr_string, NULL, 10);
                cfg->fields[field_idx].value.integer = int_value;
            } else if (cfg->fields[field_idx].type == DUMMY_FIELD_STRING) {
                cfg->fields[field_idx].value.string = strdup(fields->ptr_string);
                if (cfg->fields[field_idx].value.string == NULL) {
                    IPX_CTX_ERROR(ctx, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
                    return IPX_ERR_NOMEM;
                }
            } else if (cfg->fields[field_idx].type == DUMMY_FIELD_DOUBLE) {
                int double_value = strtod(fields->ptr_string, NULL);
                int64_t tmp_num = htobe64(le64toh(*(int64_t*)&double_value));
                double  dst_num = *(double*)&tmp_num;
                cfg->fields[field_idx].value.decimal = dst_num;
            }
            break;

        case TIMES:
            cfg->fields[field_idx].times = fields->val_uint;
            break;

        default:
            IPX_CTX_ERROR(ctx, "Unknown field in configuration (ID %d)", fields->id);
            return IPX_ERR_FORMAT;
        }

    }

    field_idx++;
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
config_parser_root(ipx_ctx_t *ctx, fds_xml_ctx_t *root, struct dummy_config *cfg)
{
    (void) ctx;
    const struct fds_xml_cont *content;

    // First, get number of new fields and allocate memory in config structure
    while (fds_xml_next(root, &content) != FDS_EOC) {
        if (content->id != FIELD) {
            // Non-field definition
            assert(false);
            continue;
        }
        cfg->fields_count++;
    }

    cfg->fields = calloc(cfg->fields_count, sizeof(*cfg->fields));
    if (!cfg->fields) {
        IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
        return IPX_ERR_FORMAT;
    }

    // Rewind pointer and start parsing
    fds_xml_rewind(root);
    while (fds_xml_next(root, &content) != FDS_EOC) {
        if (content->id != FIELD) {
            return IPX_ERR_FORMAT;
        }

        assert(content->type == FDS_OPTS_T_CONTEXT);
        if (config_parser_fields(ctx, content->ptr_ctx, cfg) != IPX_OK) {
            return IPX_ERR_FORMAT;
        }
    }

    return IPX_OK;
}

struct dummy_config *
config_parse(ipx_ctx_t *ctx, const char *params)
{
    struct dummy_config *cfg = calloc(1, sizeof(*cfg));
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

    return cfg;
}

void
config_destroy(struct dummy_config *cfg)
{
    for (uint32_t i = 0; i < cfg->fields_count; ++i) {
        if (cfg->fields[i].type == DUMMY_FIELD_STRING) {
            free(cfg->fields[i].value.string);
        }
    }
    free(cfg->fields);
    free(cfg);
}