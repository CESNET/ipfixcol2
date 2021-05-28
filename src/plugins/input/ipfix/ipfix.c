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

#include <errno.h>
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
    .dsc = "Input plugin for IPFIX File format",
    // Configuration flags (reserved for future use)
    .flags = 0,
    // Plugin version string (like "1.2.3")
    .version = "2.0.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    .ipx_min = "2.2.0"
};

/// Plugin instance data
struct plugin_data {
    /// Plugin context (log only!)
    ipx_ctx_t *ctx;
    /// Parsed plugin configuration
    struct ipfix_config *cfg;

    /// List of all files to read (matching file path)
    glob_t file_list;
    /// Index of the next file to read (see file_list->gl_pathv)
    size_t file_next_idx;

    /// Handler of the currently file
    FILE *current_file;
    /// Name/path of the current file
    const char *current_name;
    /// Transport Session identification
    struct ipx_session *current_ts;

    /// Buffer of preloaded data
    uint8_t *buffer_data;
    /// Size of the buffer
    size_t buffer_size;
    /// Valid size of the buffer
    size_t buffer_valid;
    /// Position of the reader in the buffer
    size_t buffer_offset;
};

/**
 * @brief Check if path is a directory
 *
 * @note Since we use GLOB_MARK flag, all directories ends with a slash.
 * @param[in] filename Path
 * @return True or false
 */
static inline bool
filename_is_dir(const char *filename)
{
    size_t len = strlen(filename);
    return (filename[len - 1] == '/');
}

/**
 * @brief Free list of files to read
 *
 * @param[in] list List to free
 */
static inline void
files_list_free(glob_t *list)
{
    globfree(list);
}

/**
 * @brief Get list of files to read
 *
 * @param[in]  ctx     Plugin context (log only)
 * @param[in]  pattern File pattern
 * @param[out] list    List of files
 * @return #IPX_OK on success
 * @return #IPX_ERR_NOTFOUND if no files matches the given pattern
 * @return #IPX_ERR_DENIED if the list cannot be obtained due to a read error
 * @return #IPX_ERR_NOMEM in case of a memory allocation error
 */
static inline int
files_list_get(ipx_ctx_t *ctx, const char *pattern, glob_t *list)
{
    size_t file_cnt;
#ifndef GLOB_TILDE_CHECK
#define GLOB_TILDE_CHECK GLOB_TILDE
#endif
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
        IPX_CTX_ERROR(ctx, "No file matches the given file pattern!", '\0');
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

/**
 * @brief Create a new transport session and send "open" notification
 *
 * @warning
 *   As the function sends notification to other plugins further in the pipeline, it must have
 *   permission to pass messages. Therefore, this function cannot be called within
 *   ipx_plugin_init().
 * @param[in] ctx      Plugin context (for sending notification and log)
 * @param[in] filename New file which corresponds to the new Transport Session
 * @return New transport session
 */
static struct ipx_session *
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

/**
 * @brief Close a transport session and send "close" notification
 *
 * User MUST stop using the session as it is send in a garbage message to the pipeline and
 * it will be automatically freed.
 * @warning
 *   As the function sends notification to other plugins further in the pipeline, it must have
 *   permission to pass messages. Therefore, this function cannot be called within
 *   ipx_plugin_init().
 * @param[in] ctx     Plugin context (for sending notification and log)
 * @param[in] session Transport Session to close
 */
static void
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
        /* Memory leak... We cannot destroy the session as it can be used
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

/**
 * @brief Open the next file for reading
 *
 * If any file is already opened, it will be closed and a session message (close notification)
 * will be send too. The function will try to open the next file in the list and makes sure
 * that it contains at least one IPFIX Message. Otherwise, it will be skipped and another file
 * will be used. When a suitable file is found, a new Transport Session will created and
 * particular session message (open notification) will be sent.
 *
 * @warning
 *   As the function sends notification to other plugins further in the pipeline, it must have
 *   permission to pass messages. Therefore, this function cannot be called within
 *   ipx_plugin_init().
 * @param[in] data Plugin data
 * @return #IPX_OK on success
 * @return #IPX_ERR_EOF if no more files are available
 * @return #iPX_ERR_NOMEM in case of a memory allocation error
 */
static int
next_file(struct plugin_data *data)
{
    size_t idx_next;
    size_t idx_max = data->file_list.gl_pathc;
    FILE *file_new = NULL;
    const char *name_new = NULL;

    // Signalize close of the current Transport Session
    session_close(data->ctx, data->current_ts);
    data->current_ts = NULL;
    if (data->current_file) {
        fclose(data->current_file);
        data->current_file = NULL;
        data->current_name = NULL;
    }

    // Open new file
    for (idx_next = data->file_next_idx; idx_next < idx_max; ++idx_next) {
        name_new = data->file_list.gl_pathv[idx_next];
        if (filename_is_dir(name_new)) {
            continue;
        }

        file_new = fopen(name_new, "rb");
        if (!file_new) {
            const char *err_str;
            ipx_strerror(errno, err_str);
            IPX_CTX_ERROR(data->ctx, "Failed to open '%s': %s", name_new, err_str);
            continue;
        }

        struct fds_ipfix_msg_hdr ipfix_hdr;
        if (fread(&ipfix_hdr, FDS_IPFIX_MSG_HDR_LEN, 1, file_new) != 1
                || ntohs(ipfix_hdr.version) != FDS_IPFIX_VERSION
                || ntohs(ipfix_hdr.length) < FDS_IPFIX_MSG_HDR_LEN) {
            IPX_CTX_ERROR(data->ctx, "Skipping non-IPFIX File '%s'", name_new);
            fclose(file_new);
            file_new = NULL;
            continue;
        }

        // Success
        rewind(file_new);
        break;
    }

    data->file_next_idx = idx_next + 1;
    if (!file_new) {
        return IPX_ERR_EOF;
    }

    // Signalize open of the new Transport Session
    data->current_ts = session_open(data->ctx, name_new);
    if (!data->current_ts) {
        fclose(file_new);
        return IPX_ERR_NOMEM;
    }

    IPX_CTX_INFO(data->ctx, "Reading from file '%s'...", name_new);
    data->current_file = file_new;
    data->current_name = name_new;

    data->buffer_valid = 0;
    data->buffer_offset = 0;
    return IPX_OK;
}

/**
 * @brief Get the next chunk of data
 *
 * Reads the chunk from the internal buffer with preloaded content of the file.
 * If the buffer doesn't contain required amount of data, new content will be
 * loaded from the file. Nevertheless, if the end-of-file has been reached, return
 * codes #IPX_ERR_EOF or #IPX_ERR_FORMAT might be returned.
 *
 * @param[in]  data     Plugin data
 * @param[out] out      Output buffer to fill
 * @param[in]  out_size Size of the output buffer (i.e. required amount of data)
 *
 * @return #IPX_OK on success
 * @return #IPX_ERR_EOF if the end-of-file has been reached (no more data)
 * @return #IPX_ERR_FORMAT if the end-of-file has been reached but the internal
 *   buffer doesn't contain required amount of data
 */
static int
next_chunk(struct plugin_data *data, uint8_t *out, uint16_t out_size)
{
    size_t buffer_avail = data->buffer_valid - data->buffer_offset;
    uint8_t *reader_ptr = &data->buffer_data[data->buffer_offset];

    size_t new_size;
    uint8_t *new_ptr;
    size_t ret;

    // Check if the chunk is fully in the buffer
    if (buffer_avail >= out_size) {
        memcpy(out, reader_ptr, out_size);
        data->buffer_offset += out_size;
        return IPX_OK;
    }

    // We need to load new data to the buffer
    if (buffer_avail > 0) {
        // A fragment of an unprocessed IPFIX Message must be preserved
        memcpy(data->buffer_data, reader_ptr, buffer_avail);
        data->buffer_valid = buffer_avail;
    } else {
        data->buffer_valid = 0;
    }

    new_size = data->buffer_size - data->buffer_valid;
    new_ptr = &data->buffer_data[data->buffer_valid];
    ret = fread(new_ptr, 1, new_size, data->current_file);
    data->buffer_valid += ret;
    data->buffer_offset = 0;

    // Check whether the EOF has been reached
    if (data->buffer_valid == 0 && feof(data->current_file)) {
        return IPX_ERR_EOF;
    }

    if (data->buffer_valid < out_size) {
        return IPX_ERR_FORMAT;
    }

    memcpy(out, data->buffer_data, out_size);
    data->buffer_offset += out_size;
    return IPX_OK;
}

/**
 * @brief Get the next IPFIX Message from currently opened file
 *
 * @param[in]  data Plugin data
 * @param[out] msg  IPFIX Message extracted from the file
 * @return #IPX_OK on success
 * @return #IPX_ERR_EOF if the end-of-file has been reached
 * @return #IPX_ERR_FORMAT if the file is malformed
 * @return #IPX_ERR_NOMEM in case of a memory allocation error
 */
static int
next_message(struct plugin_data *data, ipx_msg_ipfix_t **msg)
{
    struct fds_ipfix_msg_hdr ipfix_hdr;
    uint16_t ipfix_size;
    uint8_t *ipfix_data = NULL;

    struct ipx_msg_ctx ipfix_ctx;
    ipx_msg_ipfix_t *ipfix_msg;
    int ret;

    if (!data->current_file) {
        return IPX_ERR_EOF;
    }

    // Get the IPFIX Message header
    ret = next_chunk(data, (uint8_t *) &ipfix_hdr, FDS_IPFIX_MSG_HDR_LEN);
    if (ret != IPX_OK) {
        if (ret == IPX_ERR_FORMAT) {
            IPX_CTX_ERROR(data->ctx, "File '%s' is corrupted (unexpected end of file)!",
            data->current_name);
        }

        return ret;
    }

    ipfix_size = ntohs(ipfix_hdr.length);
    if (ntohs(ipfix_hdr.version) != FDS_IPFIX_VERSION
            || ipfix_size < FDS_IPFIX_MSG_HDR_LEN) {
        IPX_CTX_ERROR(data->ctx, "File '%s' is corrupted (unexpected data)!", data->current_name);
        return IPX_ERR_FORMAT;
    }

    ipfix_data = malloc(ipfix_size);
    if (!ipfix_data) {
        IPX_CTX_ERROR(data->ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }
    memcpy(ipfix_data, &ipfix_hdr, FDS_IPFIX_MSG_HDR_LEN);

    // Get the rest of the IPFIX Message body
    if (ipfix_size > FDS_IPFIX_MSG_HDR_LEN) {
        uint8_t *data_ptr =  ipfix_data + FDS_IPFIX_MSG_HDR_LEN;
        uint16_t size_remain = ipfix_size - FDS_IPFIX_MSG_HDR_LEN;

        if (next_chunk(data, data_ptr, size_remain) != IPX_OK) {
            IPX_CTX_ERROR(data->ctx, "File '%s' is corrupted (unexpected end of file)!",
                data->current_name);
            free(ipfix_data);
            return IPX_ERR_FORMAT;
        }
    }

    // Wrap the IPFIX Message
    memset(&ipfix_ctx, 0, sizeof(ipfix_ctx));
    ipfix_ctx.session = data->current_ts;
    ipfix_ctx.odid = ntohl(ipfix_hdr.odid);
    ipfix_ctx.stream = 0;

    ipfix_msg = ipx_msg_ipfix_create(data->ctx, &ipfix_ctx, ipfix_data, ipfix_size);
    if (!ipfix_msg) {
        IPX_CTX_ERROR(data->ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        free(ipfix_data);
        return IPX_ERR_NOMEM;
    }

    *msg = ipfix_msg;
    return IPX_OK;
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
    data->ctx = ctx;
    data->cfg = config_parse(ctx, params);
    if (!data->cfg) {
        free(data);
        return IPX_ERR_DENIED;
    }

    // Initialize reader buffer
    data->buffer_size = data->cfg->bsize;
    data->buffer_data = malloc(sizeof(uint8_t) * data->buffer_size);
    if (!data->buffer_data) {
        IPX_CTX_ERROR(ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        free(data);
        return IPX_ERR_DENIED;
    }

    // Prepare list of all files to read
    if (files_list_get(ctx, data->cfg->path, &data->file_list) != IPX_OK) {
        free(data->buffer_data);
        config_destroy(data->cfg);
        free(data);
        return IPX_ERR_DENIED;
    }

    ipx_ctx_private_set(ctx, data);
    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg)
{
    struct plugin_data *data = (struct plugin_data *) cfg;

    // Close the current session and file
    session_close(ctx, data->current_ts);
    if (data->current_file) {
        fclose(data->current_file);
    }

    // Final cleanup
    files_list_free(&data->file_list);
    config_destroy(data->cfg);
    free(data->buffer_data);
    free(data);
}

int
ipx_plugin_get(ipx_ctx_t *ctx, void *cfg)
{
    struct plugin_data *data = (struct plugin_data *) cfg;
    ipx_msg_ipfix_t *msg2send;

    while (true) {
        // Get a new message from the currently opened file
        switch (next_message(data, &msg2send)) {
        case IPX_OK:
            ipx_ctx_msg_pass(ctx, ipx_msg_ipfix2base(msg2send));
            return IPX_OK;
        case IPX_ERR_EOF:
        case IPX_ERR_FORMAT:
            // Open the next file
            break;
        default:
            IPX_CTX_ERROR(ctx, "Fatal error!", '\0');
            return IPX_ERR_DENIED;
        }

        // Open the next file
        switch (next_file(data)) {
        case IPX_OK:
            continue;
        case IPX_ERR_EOF:
            // No more data:
            return IPX_ERR_EOF;
        default:
            IPX_CTX_ERROR(ctx, "Fatal error!", '\0');
            return IPX_ERR_DENIED;
        }
    }
}

void
ipx_plugin_session_close(ipx_ctx_t *ctx, void *cfg, const struct ipx_session *session)
{
    struct plugin_data *data = (struct plugin_data *) cfg;
    // Do NOT dereference the session pointer because it can be already freed!
    if (session != data->current_ts) {
        // The session has been already closed
        return;
    }

    // Close the current session and file
    session_close(ctx, data->current_ts);
    if (data->current_file) {
        fclose(data->current_file);
    }

    data->current_ts = NULL;
    data->current_file = NULL;
    data->current_name = NULL;
}
