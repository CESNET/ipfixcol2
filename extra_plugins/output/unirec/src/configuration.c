/**
 * \file configuration.c
 * \author Tomas Cejka <cejkat@cesnet.cz>
 * \author Jaroslav Hlavac <hlavaj20@fit.cvut.cz>
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

#define _GNU_SOURCE
#include <stdio.h>
#include "configuration.h"

#include <ipfixcol2.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>
#include <stdbool.h>

/** Timeout configuration */
enum cfg_timeout_mode {
    CFG_TIMEOUT_WAIT = -1,      /**< Block indefinitely                     */
    CFG_TIMEOUT_NO_WAIT = -2,   /**< Don't block                            */
    CFG_TIMEOUT_HALF_WAIT = -3  /**< Block only if some client is connected */
};

/** Default maximum number of connections over TCP/TCP-TLS/Unix */
#define DEF_MAX_CONNECTIONS 64
/** Default output interface timeout                            */
#define DEF_IFC_TIMEOUT     CFG_TIMEOUT_HALF_WAIT
/** Default output buffering                                    */
#define DEF_IFC_BUFFER      true
/** Default autoflush interval (in microseconds)                */
#define DEF_IFC_AUTOFLUSH   500000

/** Parsed common TRAP parameters                  */
struct ifc_common {
    /** Automatic flush (0 == disabled)            */
    uint64_t autoflush;
    /** Data buffering and sending in large bulks  */
    bool buffer;
    /** Timeout mode                               */
    long int timeout;
};

/*
 *  <params>
 *      <uniRecFormat>DST_IP,SRC_IP,BYTES,DST_PORT,?TCP_FLAGS,SRC_PORT,PROTOCOL</uniRecFormat>
 *      <splitBiflow>true</splitBiflow>
 *      <trapIfcCommon>                                                           <!-- optional -->
 *          <timeout>NO_WAIT</timeout>                                            <!-- optional -->
 *          <buffer>true</buffer>                                                 <!-- optional -->
 *          <autoflush>500000</autoflush>                                         <!-- optional -->
 *      </trapIfcCommon>
 *
 *      <trapIfcSpec>
 *          <tcp>
 *              <port>8000</port>
 *              <maxClients>64</maxClients>                                       <!-- optional -->
 *          </tcp>
 *          <tcp-tls>
 *              <port>8000</port>
 *              <maxClients>64</maxClients>                                       <!-- optional -->
 *              <keyFile>...</keyFile>
 *              <certFile>...</certFile>
 *              <caFile>...</caFile>
 *          </tcp-tls>
 *          <unix>
 *              <name>ipfixcol-output</name>
 *              <maxClients>64</maxClients>                                       <!-- optional -->
 *          </unix>
 *          <file>
 *              <name>/tmp/nemea/trapdata</name>
 *              <mode>write</mode>                                                <!-- optional -->
 *              <time>0</time>                                                    <!-- optional -->
 *              <size>0</size>                                                    <!-- optional -->
 *          </file>
 *      </trapIfcSpec>
 *  </params>
 */

/** XML nodes */
enum params_xml_nodes {
    // Main parameters
    NODE_UNIREC_FMT = 1,
    NODE_BIFLOW_SPLIT,
    NODE_TRAP_COMMON,
    NODE_TRAP_SPEC,
    // TRAP common parameters
    COMMON_IFC_TIMEOUT,
    COMMON_FLUSH_TIMEOUT,
    COMMON_DATA_BUFFER,
    // TRAP interface specification
    SPEC_TCP,
    SPEC_TCP_TLS,
    SPEC_UNIX,
    SPEC_FILE,
    // TCP interface parameters
    TCP_PORT,
    TCP_MAX_CLIENTS,
    // TCP-TLS interface parameters
    TCP_TLS_PORT,
    TCP_TLS_MAX_CLIENTS,
    TCP_TLS_FILE_KEY,
    TCP_TLS_FILE_CERT,
    TCP_TLS_FILE_CA,
    // Unix interface parameters
    UNIX_NAME,
    UNIX_MAX_CLIENTS,
    // File interface parameters
    FILE_NAME,
    FILE_MODE,
    FILE_TIME,
    FILE_SIZE,
};

/** Definition of \<tcp\> node */
static const struct fds_xml_args args_ifc_tcp[] = {
    FDS_OPTS_ELEM(TCP_PORT,        "port",       FDS_OPTS_T_UINT, 0),
    FDS_OPTS_ELEM(TCP_MAX_CLIENTS, "maxClients", FDS_OPTS_T_UINT, FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

/** Definition of \<tcp-tls\> node */
static const struct fds_xml_args args_ifc_tcp_tls[] = {
    FDS_OPTS_ELEM(TCP_TLS_PORT,        "port",       FDS_OPTS_T_UINT,   0),
    FDS_OPTS_ELEM(TCP_TLS_MAX_CLIENTS, "maxClients", FDS_OPTS_T_UINT,   FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(TCP_TLS_FILE_KEY,    "keyFile",    FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(TCP_TLS_FILE_CERT,   "certFile",   FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(TCP_TLS_FILE_CA,     "caFile",     FDS_OPTS_T_STRING, 0),
    FDS_OPTS_END
};

/** Definition of \<unix\> node */
static const struct fds_xml_args args_ifc_unix[] = {
    FDS_OPTS_ELEM(UNIX_NAME,        "name",       FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(UNIX_MAX_CLIENTS, "maxClients", FDS_OPTS_T_UINT,   FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

/** Definition of \<file\> node */
static const struct fds_xml_args args_ifc_file[] = {
    FDS_OPTS_ELEM(FILE_NAME, "name", FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(FILE_MODE, "mode", FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(FILE_TIME, "time", FDS_OPTS_T_UINT,   FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(FILE_SIZE, "size", FDS_OPTS_T_UINT,   FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

/** Definition of \<trapIfcSpec\> node */
static const struct fds_xml_args args_trap_spec[] = {
    FDS_OPTS_NESTED(SPEC_TCP,     "tcp",     args_ifc_tcp,     FDS_OPTS_P_OPT),
    FDS_OPTS_NESTED(SPEC_TCP_TLS, "tcp-tls", args_ifc_tcp_tls, FDS_OPTS_P_OPT),
    FDS_OPTS_NESTED(SPEC_UNIX,    "unix",    args_ifc_unix,    FDS_OPTS_P_OPT),
    FDS_OPTS_NESTED(SPEC_FILE,    "file",    args_ifc_file,    FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

/** Definition of \<trapIfcCommon\> node */
static const struct fds_xml_args args_trap_common[] = {
    FDS_OPTS_ELEM(COMMON_IFC_TIMEOUT,   "timeout",   FDS_OPTS_T_STRING, FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(COMMON_FLUSH_TIMEOUT, "autoflush", FDS_OPTS_T_UINT,   FDS_OPTS_P_OPT),
    FDS_OPTS_ELEM(COMMON_DATA_BUFFER,   "buffer",    FDS_OPTS_T_BOOL,   FDS_OPTS_P_OPT),
    FDS_OPTS_END
};

/** Definition of the \<params\> node  */
static const struct fds_xml_args args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_ELEM(NODE_UNIREC_FMT,     "uniRecFormat",  FDS_OPTS_T_STRING, 0),
    FDS_OPTS_ELEM(NODE_BIFLOW_SPLIT,   "splitBiflow",   FDS_OPTS_T_BOOL,   FDS_OPTS_P_OPT),
    FDS_OPTS_NESTED(NODE_TRAP_COMMON,  "trapIfcCommon", args_trap_common,  FDS_OPTS_P_OPT),
    FDS_OPTS_NESTED(NODE_TRAP_SPEC,    "trapIfcSpec",   args_trap_spec,    0),
    FDS_OPTS_END
};

/**
 * \brief Append a string
 *
 * \note
 *   If the pointer to the location of the original string \p str contains NULL, a new string
 *   is automatically allocated.
 * \warning
 *   Pointer to a location of the original string may be different on success.
 *
 * \param[in,out] str Pointer to a location of the original string
 * \param[in]     fmt Format specifier of a new string to append
 * \param[in]     ... Variable list of arguments
 * \return #IPX_OK on success and the original string is replaced with a new one.
 * \return #IPX_ERR_NOMEM on failure and the original string is left untouched.
 */
static int
cfg_str_append(char **str, const char *fmt, ...)
{
    // New string
    char *str_add = NULL;
    int rc;

    va_list ap;
    va_start(ap, fmt);
    rc = vasprintf(&str_add, fmt, ap);
    va_end(ap);

    if (rc == -1) {
        // Failed!
        return IPX_ERR_NOMEM;
    }

    // Resize the original string
    assert(str_add != NULL);
    const size_t size_orig = ((*str) != NULL) ? strlen(*str) : 0U;
    const size_t size_add = strlen(str_add);

    char *new_str = realloc(*str, size_orig + size_add + 1); // +1 == "\0"
    if (!new_str) {
        // Failed!
        free(str_add);
        return IPX_ERR_NOMEM;
    }

    // Copy the new end
    strcpy(new_str + size_orig, str_add);
    free(str_add);

    *str = new_str;
    return IPX_OK;
}

/**
 * \brief Convert a string to long integer
 *
 * If the string contains unexpected characters, conversion fails.
 * \param[in]  str String to convert
 * \param[out] res Result
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT on failure
 */
static int
cfg_str2long(const char *str, long int *res)
{
    if (strlen(str) == 0) {
        // Empty string
        return IPX_ERR_FORMAT;
    }

    char *end_ptr = NULL;
    errno = 0;
    long int tmp = strtol(str, &end_ptr, 10);
    if ((errno == ERANGE || (tmp == LONG_MAX || tmp == LONG_MIN)) || (errno != 0 && tmp == 0)) {
        // Conversion failure
        return IPX_ERR_FORMAT;
    }

    if (*end_ptr != '\0') {
        // Unexpected characters after the number
        return IPX_ERR_FORMAT;
    }

    *res = tmp;
    return IPX_OK;
}

/**
 * \brief Remove all whitespace characters from the UniRec template specification
 * \note The returned string must be freed by user when no longer needed
 * \param[in] raw Pointer to the string to sanitize
 * \return New sanitized string or NULL (memory allocation error)
 */
static char *
cfg_str_sanitize(const char *raw)
{
    char *res = strdup(raw);
    if (!res) {
        return NULL;
    }

    size_t w_idx;
    for (w_idx = 0; (*raw) != '\0'; ++raw) {
        if (isspace((int) *raw)) {
            continue;
        }
        res[w_idx++] = *raw;
    }

    res[w_idx] = '\0';
    return res;
}

/**
 * \brief Sanitize UniRec template
 *
 * The function removes all whitespace characters and question marks
 * \note The returned string must be freed by user when no longer needed
 * \param[in] raw Template string to sanitize
 * \return New sanitized string or NULL (memory allocation error)
 */
static char *
cfg_ur_tmplt_sanitize(const char *raw)
{
    // Remove all whitespaces
    char *res = cfg_str_sanitize(raw);
    if (!res) {
        return NULL;
    }

    // Remove questions marks
    char *r_ptr, *w_ptr;
    for (r_ptr = w_ptr = res; (*r_ptr) != '\0'; r_ptr++) {
        if (*r_ptr == '?') {
            continue;
        }

        *(w_ptr++) = *r_ptr;
    }

    *w_ptr = '\0';
    return res;
}

/**
 * \brief Process \<trapIfcCommon\> node
 *
 * The function processes the particular XML node and overwrites default parameters.
 * \param[in] ctx    Instance context (just for log)
 * \param[in] root   XML context to process
 * \param[in] common Common parameters to be overwritten
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT in case of failure
 */
static int
cfg_parse_common(ipx_ctx_t *ctx, fds_xml_ctx_t *root, struct ifc_common *common)
{
    long int val;

    const struct fds_xml_cont *content;
    while (fds_xml_next(root, &content) != FDS_EOC) {
        switch (content->id) {
        case COMMON_DATA_BUFFER:
            assert(content->type == FDS_OPTS_T_BOOL);
            common->buffer = content->val_bool;
            break;
        case COMMON_FLUSH_TIMEOUT:
            assert(content->type == FDS_OPTS_T_UINT);
            common->autoflush = content->val_uint;
            break;
        case COMMON_IFC_TIMEOUT:
            assert(content->type == FDS_OPTS_T_STRING);
            if (strcasecmp(content->ptr_string, "wait") == 0) {
                common->timeout = CFG_TIMEOUT_WAIT;
            } else if (strcasecmp(content->ptr_string, "no_wait") == 0) {
                common->timeout = CFG_TIMEOUT_NO_WAIT;
            } else if (strcasecmp(content->ptr_string, "half_wait") == 0) {
                common->timeout = CFG_TIMEOUT_HALF_WAIT;
            } else if (cfg_str2long(content->ptr_string, &val) == IPX_OK && val > 0) {
                common->timeout = val;
            } else {
                IPX_CTX_ERROR(ctx, "Invalid interface timeout value '%s'", content->ptr_string);
                return IPX_ERR_FORMAT;
            }
            break;
        default:
            assert(false);
            break;
        }
    }

    return IPX_OK;
}

/**
 * \brief Process \<tcp\> or \<tcp-tls\> node
 * \param[in] ctx  Instance context (just for log)
 * \param[in] root XML context to process
 * \param[in] cfg  Parsed configuration (will be updated)
 * \param[in] type Type of the node (::SPEC_TCP or ::SPEC_TCP_TLS)
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT or #IPX_ERR_NOMEM in case of failure
 */
static int
cfg_parse_tcp(ipx_ctx_t *ctx, fds_xml_ctx_t *root, struct conf_params *cfg, enum params_xml_nodes type)
{
    assert(type == SPEC_TCP || type == SPEC_TCP_TLS);

    // Default parameters
    uint16_t port = 0;
    uint64_t max_conn = DEF_MAX_CONNECTIONS;
    char *file_key = NULL, *file_cert = NULL, *file_ca = NULL;

    const struct fds_xml_cont *content;
    while (fds_xml_next(root, &content) != FDS_EOC) {
        switch (content->id) {
        case TCP_PORT:
        case TCP_TLS_PORT:
            assert(content->type == FDS_OPTS_T_UINT);
            if (content->val_uint > UINT16_MAX) {
                IPX_CTX_ERROR(ctx, "Invalid TCP port number!", '\0');
                return IPX_ERR_FORMAT;
            }
            port = (uint16_t) content->val_uint;
            break;
        case TCP_MAX_CLIENTS:
        case TCP_TLS_MAX_CLIENTS:
            assert(content->type == FDS_OPTS_T_UINT);
            max_conn = content->val_uint;
            break;
        case TCP_TLS_FILE_CA:
            assert(content->type == FDS_OPTS_T_STRING);
            file_ca = strdup(content->ptr_string);
            break;
        case TCP_TLS_FILE_KEY:
            assert(content->type == FDS_OPTS_T_STRING);
            file_key = strdup(content->ptr_string);
            break;
        case TCP_TLS_FILE_CERT:
            assert(content->type == FDS_OPTS_T_STRING);
            file_cert = strdup(content->ptr_string);
            break;
        default:
            assert(false);
            break;
        }
    }

    if (type == SPEC_TCP_TLS) {
        // Check parameters required by TLS connection
        if (!file_ca || strlen(file_ca) == 0
            || !file_cert || strlen(file_cert) == 0
            || !file_key  || strlen(file_key)  == 0) {
            IPX_CTX_ERROR(ctx, "All files required by TCP-TLS must be specified!", '\0');
            free(file_ca); free(file_cert); free(file_key);
            return IPX_ERR_FORMAT;
        }

        if (strchr(file_ca, ':') || strchr(file_cert, ':') || strchr(file_key, ':')) {
            IPX_CTX_ERROR(ctx, "File names MUST NOT contain the colon character!");
            free(file_ca); free(file_cert); free(file_key);
            return IPX_ERR_FORMAT;
        }
    }

    // Prepare the TRAP interface specification
    char *res = NULL;
    const char *trap_ifc = (type == SPEC_TCP) ? "t" : "T";
    if (cfg_str_append(&res, "%s:%" PRIu16 ":%" PRIu64, trap_ifc, port, max_conn) != IPX_OK
        || (type == SPEC_TCP_TLS
            && cfg_str_append(&res, ":%s:%s:%s", file_key, file_cert, file_ca) != IPX_OK)) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        free(file_ca); free(file_cert); free(file_key);
        free(res);
        return IPX_ERR_NOMEM;
    }

    free(file_ca); free(file_cert); free(file_key);
    cfg->trap_ifc_spec = res;
    return IPX_OK;
}

/**
 * \brief Process \<unix\> node
 * \param[in] ctx  Instance context (just for log)
 * \param[in] root XML context to process
 * \param[in] cfg  Parsed configuration (will be updated)
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT or #IPX_ERR_NOMEM in case of failure
 */
static int
cfg_parse_unix(ipx_ctx_t *ctx, fds_xml_ctx_t *root, struct conf_params *cfg)
{
    // Default parameters
    char *name = NULL;
    uint64_t max_conn = DEF_MAX_CONNECTIONS;

    const struct fds_xml_cont *content;
    while (fds_xml_next(root, &content) != FDS_EOC) {
        switch (content->id) {
        case UNIX_NAME:
            assert(content->type == FDS_OPTS_T_STRING);
            name = strdup(content->ptr_string);
            if (!name) {
                IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
                return IPX_ERR_NOMEM;
            }
            break;
        case UNIX_MAX_CLIENTS:
            assert(content->type == FDS_OPTS_T_UINT);
            max_conn = content->val_uint;
            break;
        default:
            // Internal error
            assert(false);
        }
    }

    if (!name || strlen(name) == 0) {
        IPX_CTX_ERROR(ctx, "Unix socket name MUST be specified!", '\0');
        free(name);
        return IPX_ERR_FORMAT;
    }

    if (strchr(name, ':') != NULL) {
        IPX_CTX_ERROR(ctx, "Unix socket name MUST NOT contain the colon character!");
        free(name);
        return IPX_ERR_FORMAT;
    }

    // Prepare the TRAP interface specification
    char *res = NULL;
    if (cfg_str_append(&res, "u:%s:%" PRIu64, name, max_conn) != IPX_OK) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        free(name);
        return IPX_ERR_NOMEM;
    }

    free(name);
    cfg->trap_ifc_spec = res;
    return IPX_OK;
}

/**
 * \brief Process \<file\> node
 * \param[in] ctx  Instance context (just for log)
 * \param[in] root XML context to process
 * \param[in] cfg  Parsed configuration (will be updated)
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT or #IPX_ERR_NOMEM in case of failure
 */
static int
cfg_parse_file(ipx_ctx_t *ctx, fds_xml_ctx_t *root, struct conf_params *cfg)
{
    // Default parameters
    char *name = NULL;
    const char *mode = "w";
    uint64_t size = 0;
    uint64_t time = 0;

    // Process parameters
    const struct fds_xml_cont *content;
    while (fds_xml_next(root, &content) != FDS_EOC) {
        switch (content->id) {
        case FILE_NAME:
            // File name
            assert(content->type == FDS_OPTS_T_STRING);
            name = strdup(content->ptr_string);
            if (!name) {
                IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
                return IPX_ERR_NOMEM;
            }
            break;
        case FILE_MODE:
            // File mode
            assert(content->type == FDS_OPTS_T_STRING);
            if (strcasecmp(content->ptr_string, "append") == 0) {
                mode = "a";
            } else if (strcasecmp(content->ptr_string, "write") == 0) {
                mode = "w";
            } else {
                IPX_CTX_ERROR(ctx, "Unknown mode '%s' of the file interface!", content->ptr_string);
                free(name);
                return IPX_ERR_FORMAT;
            }
            break;
        case FILE_TIME:
            // Split time
            assert(content->type == FDS_OPTS_T_UINT);
            time = content->val_uint;
            break;
        case FILE_SIZE:
            // Split size
            assert(content->type == FDS_OPTS_T_UINT);
            size = content->val_uint;
            break;
        default:
            // Internal error
            assert(false);
        }
    }

    // Name must be always specified and MUST NOT contain a colon!
    if (!name || strlen(name) == 0) {
        IPX_CTX_ERROR(ctx, "File name MUST be specified!");
        free(name);
        return IPX_ERR_FORMAT;
    }

    if (strchr(name, ':') != NULL) {
        IPX_CTX_ERROR(ctx, "The file name MUST NOT contain the colon character!");
        free(name);
        return IPX_ERR_FORMAT;
    }

    // Prepare the TRAP interface specification
    char *res = NULL;
    if (cfg_str_append(&res, "f:%s:%s", name, mode) != IPX_OK
        || (time != 0 && cfg_str_append(&res, ":time=%" PRIu64, time) != IPX_OK)
        || (size != 0 && cfg_str_append(&res, ":size=%" PRIu64, size) != IPX_OK)) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        free(res);
        free(name);
        return IPX_ERR_NOMEM;
    }

    free(name);
    cfg->trap_ifc_spec = res;
    return IPX_OK;
}

/**
 * \brief Process \<trapIfcSpec\> node
 *
 * The function processes the particular XML node and defined TRAP interface specification.
 * \param[in] ctx  Instance context (just for log)
 * \param[in] root XML context to process
 * \param[in] cfg  Parsed configuration (will be updated)
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT or #IPX_ERR_NOMEM in case of failure
 */
static int
cfg_parse_spec(ipx_ctx_t *ctx, fds_xml_ctx_t *root, struct conf_params *cfg)
{
    int cnt = 0;

    const struct fds_xml_cont *content;
    while (fds_xml_next(root, &content) != FDS_EOC) {
        int rc;
        if (cnt > 0) {
            IPX_CTX_ERROR(ctx, "Multiple TRAP outputs are not supported!", '\0');
            return IPX_ERR_FORMAT;
        }

        switch (content->id) {
        case SPEC_TCP:
        case SPEC_TCP_TLS:
            // TCP or TCP-TLS interface
            assert(content->type == FDS_OPTS_T_CONTEXT);
            if ((rc = cfg_parse_tcp(ctx, content->ptr_ctx, cfg, content->id)) != IPX_OK) {
                return rc;
            }
            break;
        case SPEC_UNIX:
            // UNIX
            assert(content->type == FDS_OPTS_T_CONTEXT);
            if ((rc = cfg_parse_unix(ctx, content->ptr_ctx, cfg)) != IPX_OK) {
                return rc;
            }
            break;
        case SPEC_FILE:
            // File
            assert(content->type == FDS_OPTS_T_CONTEXT);
            if ((rc = cfg_parse_file(ctx, content->ptr_ctx, cfg)) != IPX_OK) {
                return rc;
            }
            break;
        default:
            // Internal error
            assert(false);
        }

        cnt++;
    }

    if (cnt == 0) {
        IPX_CTX_ERROR(ctx, "TRAP interface is not specified!", '\0');
        return IPX_ERR_FORMAT;
    }

    return IPX_OK;
}

/**
 * \brief Add common TRAP parameters to the TRAP interface specification
 *
 * \warning The interface string MUST be already specified!
 * \param[in] ctx    Instance context (just for log)
 * \param[in] cfg    Parsed configuration (will be updated)
 * \param[in] common Common TRAP parameters
 * \return #IPX_OK on success
 * \return #IPX_ERR_NOMEM in case of failure
 */
static int
cfg_add_ifc_common(ipx_ctx_t *ctx, struct conf_params *cfg, const struct ifc_common *common)
{
    int rc;
    char **ptr = &cfg->trap_ifc_spec;
    assert((*ptr) != NULL);

    // Add buffer parameter
    if (cfg_str_append(ptr, ":buffer=%s", (common->buffer) ? "on" : "off") != IPX_OK) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }

    // Add timeout parameter
    const char *timeout_str;
    switch (common->timeout) {
    case CFG_TIMEOUT_HALF_WAIT: timeout_str = "HALF_WAIT"; break;
    case CFG_TIMEOUT_NO_WAIT:   timeout_str = "NO_WAIT";   break;
    case CFG_TIMEOUT_WAIT:      timeout_str = "WAIT";      break;
    default:                    timeout_str = NULL;        break;
    }

    if (timeout_str == NULL) {
        rc = cfg_str_append(ptr, ":timeout=%ld", common->timeout);
    } else {
        rc = cfg_str_append(ptr, ":timeout=%s", timeout_str);
    }

    if (rc != IPX_OK) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }

    // Add autoflush parameter
    if (common->autoflush == 0) {
        rc = cfg_str_append(ptr, ":autoflush=off");
    }  else {
        rc = cfg_str_append(ptr, ":autoflush=%" PRIu64, common->autoflush);
    }

    if (rc != IPX_OK) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }

    return IPX_OK;
}

/**
 * \brief Process \<params\> node
 *
 * The function processes the particular XML node and updates all configuration parameters.
 * \param[in] ctx  Instance context (just for log)
 * \param[in] root XML context to process
 * \param[in] cfg  Parsed configuration (will be updated)
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT or #IPX_ERR_NOMEM in case of failure
 */
static int
cfg_parse_params(ipx_ctx_t *ctx, fds_xml_ctx_t *root, struct conf_params *cfg)
{
    int rc;

    // Set default values
    cfg->biflow_split = true;

    // Set default TRAP common parameters
    struct ifc_common common;
    common.autoflush = DEF_IFC_AUTOFLUSH;
    common.buffer =    DEF_IFC_BUFFER;
    common.timeout =   DEF_IFC_TIMEOUT;

    // Parse configuration
    const struct fds_xml_cont *content;
    while (fds_xml_next(root, &content) != FDS_EOC) {
        switch (content->id) {
        case NODE_UNIREC_FMT:
            // UniRec output format
            assert(content->type == FDS_OPTS_T_STRING);
            cfg->unirec_fmt = cfg_ur_tmplt_sanitize(content->ptr_string);
            cfg->unirec_spec = cfg_str_sanitize(content->ptr_string);
            if (cfg->unirec_fmt == NULL || cfg->unirec_spec == NULL) {
                IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
                return IPX_ERR_NOMEM;
            }
            break;
        case NODE_BIFLOW_SPLIT:
            // Split biflow
            assert(content->type == FDS_OPTS_T_BOOL);
            cfg->biflow_split = content->val_bool;
            break;
        case NODE_TRAP_SPEC:
            // TRAP output interface specifier
            assert(content->type == FDS_OPTS_T_CONTEXT);
            if ((rc = cfg_parse_spec(ctx, content->ptr_ctx, cfg)) != IPX_OK) {
                return rc;
            }
            break;
        case NODE_TRAP_COMMON:
            // TRAP common interface parameters
            assert(content->type == FDS_OPTS_T_CONTEXT);
            if ((rc = cfg_parse_common(ctx, content->ptr_ctx, &common)) != IPX_OK) {
                return rc;
            }
            break;
        default:
            // Internal error
            assert(false);
        }
    }

    // Add TRAP common parameters
    if ((rc = cfg_add_ifc_common(ctx, cfg, &common)) != IPX_OK) {
        return rc;
    }

    return IPX_OK;
}

/**
 * \brief Validate configuration
 * \param[in] ctx Instance context (just for log)
 * \param[in] cfg Configuration to validate
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT on failure
 */
static int
cfg_validate(ipx_ctx_t *ctx, const struct conf_params *cfg)
{
    int rc = IPX_OK;

    if (!cfg->trap_ifc_spec || strlen(cfg->trap_ifc_spec) == 0) {
        IPX_CTX_ERROR(ctx, "TRAP interface is not specified!", '\0');
        rc = IPX_ERR_FORMAT;
    }

    if (!cfg->unirec_fmt || strlen(cfg->unirec_fmt) == 0
        || !cfg->unirec_spec || strlen(cfg->unirec_spec) == 0) {
        IPX_CTX_ERROR(ctx, "UniRec template is not specified!", '\0');
        rc = IPX_ERR_FORMAT;
    }

    return rc;
}

// ---------------------------------------------------------------------------------------------

struct conf_params *
configuration_parse(ipx_ctx_t *ctx, const char *params)
{
    struct conf_params *cnf = calloc(1, sizeof(*cnf));
    if (!cnf) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        return NULL;
    }

    // Try to parse the plugin configuration
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
    int rc = cfg_parse_params(ctx, params_ctx, cnf);
    fds_xml_destroy(parser);
    if (rc != IPX_OK) {
        configuration_free(cnf);
        return NULL;
    }

    // Check combinations
    if (cfg_validate(ctx, cnf) != IPX_OK) {
        // Failed!
        configuration_free(cnf);
        return NULL;
    }

    return cnf;
}

void
configuration_free(struct conf_params *cfg)
{
    if (!cfg) {
        return;
    }

    free(cfg->trap_ifc_spec);
    free(cfg->unirec_fmt);
    free(cfg->unirec_spec);
    free(cfg);
}
