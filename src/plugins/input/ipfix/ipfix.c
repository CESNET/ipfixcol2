/**
 * \file src/plugins/input/tcp/tcp.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief IPFIX File input plugin for IPFIXcol
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

#include <glob.h>
#include <ipfixcol2.h>
#include <stdlib.h>
#include <stdio.h>  // fopen, fclose

#include "config.h"

/// Plugin description
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin type
    .type = IPX_PT_INPUT,
    // Plugin identification name
    .name = "ipfix",
    // Brief description of plugin
    .dsc = "Input plugin for IPFIX File.",
    // Configuration flags (reserved for future use)
    .flags = 0,
    // Plugin version string (like "1.2.3")
    .version = "2.0.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    .ipx_min = "2.2.0"
};

/// Plugin instance data
struct plugin_data {
    /// Parsed plugin configuration
    struct ipfix_config *cfg;

    /// List of all files to read (matching file path)
    glob_t file_list;
    /// Index of the next file to read (see file_list->gl_pathv)
    size_t file_next_idx;

    /// Currently opened file
    FILE *current_file;
    /// Transport Session identification of the
    struct ipx_session *ts;
};

static inline bool
filename_is_dir(const char *filename)
{
    size_t len = strlen(filename);
    return (filename[len - 1] == '/');
}

static inline int
files_list_free(glob_t *list)
{
    globfree(list);
}

static inline int
files_list_get(ipx_ctx_t *ctx, const char *pattern, glob_t *list)
{
    size_t file_cnt;
    int glob_flags = GLOB_MARK | GLOB_BRACE | GLOB_TILDE_CHECK;
    int rc = glob(pattern, glob_flags, NULL, list);

    switch (rc) {
    case 0: // Success
        break;
    case GLOB_NOSPACE:
        IPX_CTX_ERROR(ctx, "Failed to list files to process due memory allocation error!", '\0');
        return IPX_ERR_NOMEM;
    case GLOB_ABORTED:
        IPX_CTX_ERROR(ctx, "Failed to list files to process due read error", '\0');
        return IPX_ERR_DENIED;
    case GLOB_NOMATCH:
        IPX_CTX_ERROR(ctx, "No files matches the given file pattern!", '\0');
        return IPX_ERR_NOTFOUND;
    default:
        IPX_CTX_ERROR(ctx, "glob() failed and returned unexpected value!", '\0');
        return IPX_ERR_DENIED;
    }

    file_cnt = 0;
    for (size_t i = 0; i < list->gl_pathc; ++i) {
        const char *filename = list->gl_pathv[i];
        if (filename_is_dir(filename)) {
            continue;
        }

        file_cnt++;
    }

    if (!file_cnt) {
        IPX_CTX_ERROR(ctx, "No files matches the given file pattern!", '\0');
        files_list_free(list);
        return IPX_ERR_NOTFOUND;
    } else {
        IPX_CTX_INFO(ctx, "%zu file(s) will be processed", file_cnt);
    }

    return IPX_OK;
}

struct ipx_session *
session_open(ipx_ctx_t *ctx, const char *filename)
{
    struct ipx_session *res;
    struct ipx_msg_session *msg;

    // Create the session structure
    res = ipx_session_new_file(filename);
    if (!res) {
        return NULL;
    }

    // Notify plugins further in the pipeline about the new session
    msg = ipx_msg_session_create(res, IPX_MSG_SESSION_OPEN);
    if (!msg) {
        ipx_session_destroy(res);
        return NULL;
    }

    if (ipx_ctx_msg_pass(ctx, ipx_msg_session2base(msg)) != IPX_OK) {
        ipx_msg_session_destroy(msg);
        ipx_session_destroy(res);
        return NULL;
    }

    return res;
}

void
session_close(ipx_ctx_t *ctx, struct ipx_session *session)
{
    ipx_msg_session_t *msg_session;
    ipx_msg_garbage_t *msg_garbage;
    ipx_msg_garbage_cb garbage_cb = (ipx_msg_garbage_cb) &ipx_session_destroy;

    if (!session) {
        // Nothing to do
        return;
    }

    msg_session = ipx_msg_session_create(session, IPX_MSG_SESSION_CLOSE);
    if (!msg_session) {
        IPX_CTX_ERROR(ctx, "Failed to close a Transport Session", '\0');
        return;
    }

    if (ipx_ctx_msg_pass(ctx, ipx_msg_session2base(msg_session)) != IPX_OK) {
        IPX_CTX_ERROR(ctx, "Failed to pass close notification of a Transport Session", '\0');
        ipx_msg_session_destroy(msg_session);
        return;
    }

    msg_garbage = ipx_msg_garbage_create(session, garbage_cb);
    if (!msg_garbage) {
        /* Memory leak... We cannot destroy the session as it can used by
         * by other plugins further in the pipeline.
         */
        IPX_CTX_ERROR(ctx, "Failed to create a garbage message with a Transport Session", '\0');
        return;
    }

    if (ipx_ctx_msg_pass(ctx, ipx_msg_garbage2base(msg_garbage)) != IPX_OK) {
        /* Memory leak... We cannot destroy the message as it also destroys
         * the session structure.
         */
        IPX_CTX_ERROR(ctx, "Failed to pass a garbage message with a Transport Session", '\0');
        return;
    }
}

// -------------------------------------------------------------------------------------------------


int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params)
{
    struct plugin_data *data = calloc(1, sizeof(*data));
    if (!data) {
        IPX_CTX_ERROR(ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        return IPX_ERR_DENIED;
    }

    // Parse configuration
    data->cfg = config_parse(ctx, params);
    if (!data->cfg) {
        free(data);
        return IPX_ERR_DENIED;
    }

    // Prepare list of all files to read
    if (files_list_get(ctx, data->cfg->path, &data->file_list) != IPX_OK) {
        config_destroy(data->cfg);
        free(data);
    }

    ipx_ctx_private_set(ctx, data);
    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg)
{
    struct plugin_data *data = (struct plugin_data *) cfg;

    // Close the current session and file
    session_close(ctx, data->ts);
    if (data->current_file) {
        fclose(data->current_file);
    }

    // Final cleanup
    files_list_free(&data->file_list);
    config_destroy(data->cfg);
    free(data);
}

int
ipx_plugin_get(ipx_ctx_t *ctx, void *cfg)
{
    struct plugin_data *data = (struct plugin_data *) cfg;

    // dummy code
    size_t file_idx = data->file_next_idx;
    if (file_idx == data->file_list.gl_pathc) {
        return IPX_ERR_EOF;
    }

    const char *file_path = data->file_list.gl_pathv[file_idx];
    IPX_CTX_INFO(ctx, "New session: %s", file_path);
    data->ts = session_open(ctx, file_path);
    if (!data->ts) {
        return IPX_ERR_DENIED;
    }
    session_close(ctx, data->ts);
    data->ts = NULL;
    data->file_next_idx++;

    return IPX_OK;
}

/*
void
ipx_plugin_session_close(ipx_ctx_t *ctx, void *cfg, const struct ipx_session *session)
{
    struct tcp_data *data = (struct tcp_data *) cfg;
    // Do NOT dereference the session pointer because it can be already freed!

    // TODO: if matches the current file, skip it and open the next one...
}
*/