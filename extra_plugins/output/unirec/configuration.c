/**
 * \file configuration.c
 * \author Tomas Cejka <cejkat@cesnet.cz>
 * \author Jaroslav Hlavac <hlavaj20@fit.cvut.cz>
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

#define _GNU_SOURCE
#include <stdio.h>
#include "configuration.h"
#include "unirecplugin.h"

#include <ipfixcol2.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/* <!-- one instance of UniRec -->
 * <params>
  *     <!-- TRAP interface type. t is for TCP see supervisor + TLS-->
  *     <trapIfcType>TCP</trapIfcType>
  *     <!-- TRAP interface port. UniRec flows will be sent to this port -->
  *     <trapIfcSocket>8000</trapIfcSocket>
  *     <!-- TRAP interface timeout. 0 is for TRAP_NO_WAIT (non-blocking) -->
  *     <trapIfcTimeout>0</trapIfcTimeout>                                        <!-- optional -->
  *     <!-- TRAP interface flush timeout in micro seconds -->
  *     <trapIfcFlushTimeout>10000000</trapIfcFlushTimeout>                       <!-- optional -->
  *     <!-- TRAP interface buffer switch. 1 is for ON -->
  *     <trapIfcBufferSwitch>1</trapIfcBufferSwitch>                              <!-- optional -->
  *     <!-- TRAP interface UniRec template, elements marked with '?' are optional and might not be filled (e.g. TCP_FLAGS) -->
  *     <UniRecFormat>DST_IP,SRC_IP,BYTES,DST_PORT,?TCP_FLAGS,SRC_PORT,PROTOCOL</UniRecFormat>
  * </params>
  */

/* XML nodes */
enum params_xml_nodes {
    NODE_TRAP_IFC_TYPE = 1,
    NODE_TRAP_IFC_SOCKET,
    NODE_TRAP_IFC_TIMEOUT,
    NODE_TRAP_IFC_FLUSH_TIMEOUT,
    NODE_TRAP_IFC_BUFFER_SWITCH,
    NODE_UNIREC_FORMAT
};

/** Definition of the \<params\> node  */
static const struct fds_xml_args args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_ELEM(NODE_TRAP_IFC_TYPE,          "trapIfcType",         FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(NODE_TRAP_IFC_SOCKET,        "trapIfcSocket",       FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(NODE_TRAP_IFC_TIMEOUT,       "trapIfcTimeout",      FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(NODE_TRAP_IFC_FLUSH_TIMEOUT, "trapIfcFlushTimeout", FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(NODE_TRAP_IFC_BUFFER_SWITCH, "trapIfcBufferSwitch", FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(NODE_UNIREC_FORMAT,          "UniRecFormat",        FDS_OPTS_T_STRING, 0),
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

    if (!cfg->trap_ifc_type) {
        IPX_CTX_ERROR(ctx, "Trap interface type is not set.", '\0');
        ret_code = IPX_ERR_FORMAT;
    }

    if (!cfg->trap_ifc_socket) {
        IPX_CTX_ERROR(ctx, "Trap interface socket is not set.", '\0');
        ret_code = IPX_ERR_FORMAT;
    }

    if (!cfg->unirec_format) {
        IPX_CTX_ERROR(ctx, "Unirec format is not set.", '\0');
        ret_code = IPX_ERR_FORMAT;
    }
    return ret_code;
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
        case NODE_TRAP_IFC_TYPE:
            assert(content->type == FDS_OPTS_T_STRING);
            if (strcmp(content->ptr_string, "UNIXSOCKET") == 0) {
                cnf->trap_ifc_type = 'u';
            } else if (strcmp(content->ptr_string, "TCP") == 0) {
                cnf->trap_ifc_type = 't';
            } else if (strcmp(content->ptr_string, "TLS") == 0) {
                cnf->trap_ifc_type = 'T';
            } else if (strcmp(content->ptr_string, "FILE") == 0) {
                cnf->trap_ifc_type = 'f';
            } else if (strcmp(content->ptr_string, "BLACKHOLE") == 0) {
                cnf->trap_ifc_type = 'b';
            } else {
                IPX_CTX_ERROR(ctx, "Unsupported trapIfcType.");
            }
            break;
        case NODE_TRAP_IFC_SOCKET:
            assert(content->type == FDS_OPTS_T_STRING);
            cnf->trap_ifc_socket = strdup(content->ptr_string);
            if(cnf->trap_ifc_socket == NULL) {
                IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
                return IPX_ERR_NOMEM;
            }
            break;
        case NODE_TRAP_IFC_TIMEOUT:
            assert(content->type == FDS_OPTS_T_STRING);
            cnf->trap_ifc_timeout = strdup(content->ptr_string);
            if(cnf->trap_ifc_timeout == NULL) {
                IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
                return IPX_ERR_NOMEM;
            }
            break;
        case NODE_TRAP_IFC_FLUSH_TIMEOUT:
            assert(content->type == FDS_OPTS_T_STRING);
            cnf->trap_ifc_autoflush = strdup(content->ptr_string);
            if(cnf->trap_ifc_autoflush == NULL) {
                IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
                return IPX_ERR_NOMEM;
            }
            break;
        case NODE_TRAP_IFC_BUFFER_SWITCH:
            assert(content->type == FDS_OPTS_T_STRING);
            cnf->trap_ifc_bufferswitch = strdup(content->ptr_string);
            if(cnf->trap_ifc_bufferswitch == NULL) {
                IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
                return IPX_ERR_NOMEM;
            }
            break;
        case NODE_UNIREC_FORMAT:
            assert(content->type == FDS_OPTS_T_STRING);
            cnf->unirec_format = strdup(content->ptr_string);
            if(cnf->unirec_format == NULL) {
                IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
                return IPX_ERR_NOMEM;
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

    if (config->trap_ifc_socket) {
        free(config->trap_ifc_socket);
        config->trap_ifc_socket = NULL;
    }

    if (config->trap_ifc_timeout) {
        free(config->trap_ifc_timeout);
        config->trap_ifc_timeout = NULL;
    }

    if (config->trap_ifc_autoflush) {
        free(config->trap_ifc_autoflush);
        config->trap_ifc_autoflush = NULL;
    }

    if (config->trap_ifc_bufferswitch) {
        free(config->trap_ifc_bufferswitch);
        config->trap_ifc_bufferswitch = NULL;
    }

    if (config->unirec_format) {
        free(config->unirec_format);
        config->unirec_format = NULL;
    }

    free(config);
}

char *
configuration_create_ifcspec(ipx_ctx_t *ctx, const struct conf_params *parsed_params)
{
    char *ifc_spec, *tmp_spec;
    int ret = asprintf(&ifc_spec, "%c:%s", parsed_params->trap_ifc_type, parsed_params->trap_ifc_socket);
    if (ret == -1) {
        IPX_CTX_ERROR(ctx, "Unable to create IFC spec (%s:%d)", __FILE__, __LINE__);
        return NULL;
    }

    if (parsed_params->trap_ifc_bufferswitch) {
        int ret = asprintf(&tmp_spec, "%s:buffer=%s", ifc_spec, parsed_params->trap_ifc_bufferswitch);
        if (ret == -1) {
            IPX_CTX_ERROR(ctx, "Unable to create IFC spec (%s:%d)", __FILE__, __LINE__);
            free(ifc_spec);
            return NULL;
        } else {
            free(ifc_spec);
            ifc_spec = tmp_spec;
            tmp_spec = NULL;
        }
    }

    if (parsed_params->trap_ifc_autoflush) {
        int ret = asprintf(&tmp_spec, "%s:autoflush=%s", ifc_spec, parsed_params->trap_ifc_autoflush);
        if (ret == -1) {
            IPX_CTX_ERROR(ctx, "Unable to create IFC spec (%s:%d)", __FILE__, __LINE__);
            free(ifc_spec);
            return NULL;
        } else {
            free(ifc_spec);
            ifc_spec = tmp_spec;
            tmp_spec = NULL;
        }
    }

    if (parsed_params->trap_ifc_timeout) {
        int ret = asprintf(&tmp_spec, "%s:timeout=%s", ifc_spec, parsed_params->trap_ifc_timeout);
        if (ret == -1) {
            IPX_CTX_ERROR(ctx, "Unable to create IFC spec (%s:%d)", __FILE__, __LINE__);
            free(ifc_spec);
            return NULL;
        } else {
            free(ifc_spec);
            ifc_spec = tmp_spec;
            tmp_spec = NULL;
        }
    }
    return ifc_spec;
}

