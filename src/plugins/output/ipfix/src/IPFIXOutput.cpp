/**
 * \file src/plugins/output/ipfix/src/IPFIXOutput.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief IPFIX output plugin logic
 * \date 2019
 */

/* Copyright (C) 2019 CESNET, z.s.p.o.
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

#include "IPFIXOutput.hpp"

#include <stdexcept>
#include <algorithm>
#include <iterator>
#include <cstring>
#include <climits>
#include <ctime>
#include <cinttypes>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <libfds.h>

/**
 * \brief Check whether a new file should be created
 * \param[in] export_time Current export time
 * \return true or false
 */
bool
IPFIXOutput::should_start_new_file(std::time_t current_time)
{
    if (config->window_size == 0 && output_file != nullptr) {
        return false;
    }

    return (current_time >= file_start_time + time_t(config->window_size));
}

/**
 * \brief Create a new output file
 * \param[in] current_time Current export time
 * \throw runtime_error if the file cannot be created
 */
void
IPFIXOutput::new_file(const std::time_t current_time)
{
    // Require (Options) Templates definitions to be added (only if this is not the first file)
    bool add_tmplts = (output_file != nullptr);

    // Close the previous file, if exists
    close_file();

    // Get the timestamp of the file to create
    if (config->align_windows) {
        // Round down to the nearest multiple of window size
        file_start_time = (current_time / config->window_size) * config->window_size;
    } else {
        file_start_time = current_time;
    }

    std::tm ts_now;
    std::tm *ts_res = config->use_localtime
        ? localtime_r(&file_start_time, &ts_now)
        : gmtime_r(&file_start_time, &ts_now);
    if (ts_res == nullptr) {
        // An error has occurred
        const char *err_str;
        ipx_strerror(errno, err_str);
        throw std::runtime_error("Failed to convert time (localtime/gmtime failed): "
            + std::string(err_str));
    }

    // Create the filename
    constexpr size_t FILENAME_MAXSIZE = PATH_MAX;
    char filename[FILENAME_MAXSIZE];

    if (std::strftime(filename, FILENAME_MAXSIZE, config->filename.c_str(), &ts_now) == 0) {
        std::string limit = std::to_string(FILENAME_MAXSIZE);
        throw std::runtime_error("Max filename size exceeded (" + limit + " B)!");
    }

    // Create a directory (if doesn't exist)
    const char *path_dir_only;
    std::unique_ptr<char, decltype(&free)> path_cpy(strdup(filename), &free);
    if (!path_cpy) {
        throw std::runtime_error("Memory allocation failed");
    }

    path_dir_only = dirname(path_cpy.get());
    if (ipx_utils_mkdir(path_dir_only, IPX_UTILS_MKDIR_DEF) != IPX_OK) {
        const char *err_str;
        ipx_strerror(errno, err_str);
        throw std::runtime_error("Failed to create directory '" + std::string(path_dir_only)
            + "':" + std::string(err_str));
    }

    // Open the file for writing
    output_file = std::fopen(filename, "w");
    if (!output_file) {
        const char *err_str;
        ipx_strerror(errno, err_str);
        throw std::runtime_error("Failed to create file '" + std::string(filename)
            + "': " + std::string(err_str));
    }

    // Consider all Templates as undefined
    for (auto &odid_pair : odid_contexts) {
        odid_pair.second.needs_to_write_templates = add_tmplts;
    }

    IPX_CTX_INFO(plugin_context, "New output file created: %s", filename);
}

/**
 * \brief Close current output file
 */
void
IPFIXOutput::close_file()
{
    if (!output_file) {
        return;
    }

    int return_code = std::fclose(output_file);
    if (return_code != 0) {
        IPX_CTX_WARNING(plugin_context, "Error closing output file", '\0');
    }

    output_file = nullptr;
    IPX_CTX_INFO(plugin_context, "Closed output file", '\0');
}

/// Auxiliary data structure for callback function
struct write_templates_aux {
    std::FILE *file;                   ///< Output file

    uint32_t msg_odid;                 ///< IPFIX Message - ODID
    uint32_t msg_etime;                ///< IPFIX Message - Export Time
    uint32_t msg_seqnum;               ///< IPFIX Message - Sequence number

    uint8_t *buffer;                   ///< IPFIX Message buffer
    uint16_t mem_used;                 ///< Size of valid data in the buffer

    struct fds_ipfix_set_hdr *set_ptr; ///< Pointer to the current IPFIX Set
    enum fds_template_type set_type;   ///< Type of the templates in the current IPFIX Set
    uint16_t set_size;                 ///< Size of the current IPFIX Set
};

/**
 * \brief Auxiliary function for writing IPFIX Message with (Options) Templates to the file
 * \param[in] Callback data (file, message buffer, etc.)
 */
static void
write_template_dump(struct write_templates_aux &ctx)
{
    if (ctx.mem_used <= sizeof(fds_ipfix_msg_hdr) + sizeof(fds_ipfix_set_hdr)) {
        // Nothing to do
        return;
    }

    // Update the IPFIX Message header and IPFIX Set header
    auto *msg_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr *>(ctx.buffer);
    msg_hdr->length = htons(ctx.mem_used);
    ctx.set_ptr->length = htons(ctx.set_size);

    // Write the message to the file
    std::fwrite(ctx.buffer, ctx.mem_used, 1, ctx.file);
}

/**
 * \brief Auxiliary callback function that stores (Options) Template to an IPFIX Message
 *
 * \warning This function is called through C callback i.e. no exception can be thrown!
 * \param[in] tmplt IPFIX (Options) Template to store
 * \param[in] data  Callback data (file, message buffer, etc.)
 * \return Always returns true
 */
static bool
write_templates_cb(const struct fds_template *tmplt, void *data)
{
    const uint16_t tmplt_size = tmplt->raw.length;

    // Each template must fit into at least empty IPFIX Message
    assert(tmplt_size < UINT16_MAX - sizeof(fds_ipfix_msg_hdr) - sizeof(fds_ipfix_set_hdr));
    assert(tmplt->type == FDS_TYPE_TEMPLATE || tmplt->type == FDS_TYPE_TEMPLATE_OPTS);

    auto *ctx = reinterpret_cast<struct write_templates_aux *>(data);
    constexpr size_t MSG_SIZE = 1400; // Create only IPFIX Message with at most this length

    // First, check if the template can fit into the current message
    size_t size_needed = 0;
    size_needed += (tmplt->type != ctx->set_type) ? FDS_IPFIX_SET_HDR_LEN : 0;
    size_needed += tmplt_size;
    if (ctx->mem_used != 0 && ctx->mem_used + size_needed > MSG_SIZE) {
        // Update the header(s) and write the IPFIX Message to the file
        write_template_dump(*ctx);
        ctx->mem_used = 0;
    }

    if (ctx->mem_used == 0) {
        // Add IPFIX Message header (length will be filled later)
        auto *ipx_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr *>(ctx->buffer);
        ipx_hdr->version = htons(FDS_IPFIX_VERSION);
        ipx_hdr->export_time = htonl(ctx->msg_etime);
        ipx_hdr->seq_num = htonl(ctx->msg_seqnum);
        ipx_hdr->odid = htonl(ctx->msg_odid);

        ctx->mem_used = FDS_IPFIX_MSG_HDR_LEN;

        ctx->set_type = FDS_TYPE_TEMPLATE_UNDEF;
        ctx->set_ptr = nullptr;
        ctx->set_size = 0;
    }

    if (ctx->set_type != tmplt->type) {
        // The template type is different -> create a new IPFIX (Options) Template Set
        if (ctx->set_ptr != nullptr) {
            // Update the length of the last Set
            ctx->set_ptr->length = htons(ctx->set_size);
        }

        // Create a new IPFIX (Options) Template Set
        uint16_t set_id = (tmplt->type == FDS_TYPE_TEMPLATE)
            ? FDS_IPFIX_SET_TMPLT        // Template Set
            : FDS_IPFIX_SET_OPTS_TMPLT;  // Options Template Set
        ctx->set_ptr = reinterpret_cast<struct fds_ipfix_set_hdr *>(ctx->buffer + ctx->mem_used);
        ctx->set_ptr->flowset_id = htons(set_id); // Length will be filled later
        ctx->set_type = tmplt->type;
        ctx->set_size = FDS_IPFIX_SET_HDR_LEN;
        ctx->mem_used += FDS_IPFIX_SET_HDR_LEN;
    }

    // Copy the template
    std::memcpy(ctx->buffer + ctx->mem_used, tmplt->raw.data, tmplt_size);
    ctx->mem_used += tmplt_size;
    ctx->set_size += tmplt_size;
    return true;
}

/**
 * \brief Store all currently valid (Options) Templates as one or more IPFIX Messages
 *
 * The function goes through all (Options) Templates, creates valid IPFIX Messages with
 * these (Options) Templates and writes them to the file.
 * \param[in] snap     Template snapshot with all (Options) Templates to store
 * \param[in] odid     Observation Domain ID to which the (Options) Templates belong
 * \param[in] exp_time Export Time of the IPFIX Messages
 * \param[in] seq_num  Sequence number of the IPFIX Messages
 */
void
IPFIXOutput::write_templates(const fds_tsnapshot_t *snap, uint32_t odid, uint32_t exp_time,
    uint32_t seq_num)
{
    struct write_templates_aux cb_data;
    cb_data.file = output_file;
    cb_data.msg_odid = odid;
    cb_data.msg_etime = exp_time;
    cb_data.msg_seqnum = seq_num;
    cb_data.buffer = buffer.get();
    cb_data.mem_used = 0;
    cb_data.set_ptr = nullptr;
    cb_data.set_type = FDS_TYPE_TEMPLATE_UNDEF;
    cb_data.set_size = 0;

    // Iterate over all valid (Options) Templates and write them to the file as IPFIX Messages
    fds_tsnapshot_for(snap, &write_templates_cb, (void *)&cb_data);

    // Write the last message to the file
    write_template_dump(cb_data);
}

/**
 * \brief Get a context of a given Observation Domain ID
 * \note A new context is created if the context doesn't exist.
 * \param[in] odid    Observation Domain ID
 * \param[in] session Transport Session that wants to write to the file
 * \return Pointer or nullptr (the Transport Session doesn't have right to write into the file)
 */
struct IPFIXOutput::odid_context_s *
IPFIXOutput::get_odid(uint32_t odid, const ipx_session *session)
{
    auto *odid_ctx = &odid_contexts[odid];
    if (odid_ctx->session == nullptr) {
        // Grant this Transport Session access to write into the file with the given ODID...
        odid_ctx->session = session;
        IPX_CTX_INFO(plugin_context, "[ODID: %" PRIu32 "] '%s' has been granted access to write to "
            "the file with the given ODID.", odid, session->ident);
        return odid_ctx;
    }

    if (odid_ctx->session == session) {
        // This is already known Transport Session with access to write into the file
        return odid_ctx;
    }

    // Only Transport Sessions with ODID collision (and no rights to write into file) goes here...
    const auto it = odid_ctx->colliding_sessions.find(session);
    if (it != odid_ctx->colliding_sessions.end()) {
        return nullptr;
    }

    // We only want to print warning message once
    const char *session_allowed = odid_ctx->session->ident;
    const char *session_blocked = session->ident;

    IPX_CTX_WARNING(plugin_context, "[ODID: %" PRIu32 "] ODID collision between '%s' and '%s' was "
        "detected! IPFIX Messages from '%s' with the given ODID will dropped until disconnection "
        "of the colliding session!",
        odid, session_allowed, session_blocked, session_blocked);
    odid_ctx->colliding_sessions.insert(session);
    return nullptr;
}

/**
 * \brief Processes an incoming IPFIX message from the collector
 * \param[in] message  The IPFIX message
 * \throws runtime_error if a new file cannot be created
 */
void
IPFIXOutput::on_ipfix_message(ipx_msg_ipfix *message)
{
    // Prepare info about the IPFIX Message
    const auto *msg_ctx = ipx_msg_ipfix_get_ctx(message);
    const auto *msg_hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(ipx_msg_ipfix_get_packet(message));

    const uint32_t msg_etime = ntohl(msg_hdr->export_time);
    const uint32_t msg_seq = ntohl(msg_hdr->seq_num);
    const uint32_t msg_odid = ntohl(msg_hdr->odid);
    const uint16_t msg_size = ntohs(msg_hdr->length);

    // Find context corresponding to the ODID
    odid_context_s *odid_context = get_odid(msg_odid, msg_ctx->session);
    if (!odid_context) {
        // The session is in collision with another session -> drop the message
        return;
    }

    // Start a new file, if needed
    std::time_t time_now = (config->split_on_export_time)
        ? time_t(msg_etime)       // Warning: assigning uint32_t to time_t
        : std::time(NULL);
    assert(time_now >= 0 && sizeof(time_now) >= sizeof(msg_etime));

    if (should_start_new_file(time_now)) {
        new_file(time_now); // This will make sure that templates will be written to the file
    }

    // We need a templates snapshot from a Data Record, so at least one is needed!
    const uint32_t drec_cnt = ipx_msg_ipfix_get_drec_cnt(message);
    const fds_tsnapshot_t *tsnap = nullptr;
    if (drec_cnt > 0) {
        const auto *drec = ipx_msg_ipfix_get_drec(message, 0);
        tsnap = drec->rec.snap;
    }

    // Write all (Options) Templates, if required
    if (tsnap != nullptr && odid_context->needs_to_write_templates) {
        uint32_t new_sn = (config->preserve_original) ? msg_seq : odid_context->sequence_number;
        write_templates(tsnap, msg_odid, msg_etime, new_sn);
        odid_context->needs_to_write_templates = false;
    }

    // If we don't have to look for unknown Data Sets, just copy the whole message -> FAST PATH
    if (config->preserve_original) {
        std::fwrite(msg_hdr, msg_size, 1, output_file);
        return;
    }

    // SLOW PATH - check if the IPFIX Message is fully known and modify it, if necessary

    // Copy the IPFIX Message header to the buffer
    auto *new_hdr = reinterpret_cast<struct fds_ipfix_msg_hdr *>(buffer.get());
    std::memcpy(buffer.get(), msg_hdr, FDS_IPFIX_MSG_HDR_LEN);
    uint16_t new_pos = FDS_IPFIX_MSG_HDR_LEN;

    // Iterate over all IPFIX Sets in the IPFIX Message
    struct ipx_ipfix_set *sets_data;
    size_t sets_count;
    ipx_msg_ipfix_get_sets(message, &sets_data, &sets_count);

    for (size_t i = 0; i < sets_count; ++i) {
        const struct fds_ipfix_set_hdr *set = sets_data[i].ptr;
        const uint16_t set_id = ntohs(set->flowset_id);
        const uint16_t set_len = ntohs(set->length);

        if (set_id < FDS_IPFIX_SET_MIN_DSET) {
            // Not a Data Sets -> just copy
            std::memcpy(buffer.get() + new_pos, set, set_len);
            new_pos += set_len;
            continue;
        }

        // Data Sets only
        const uint8_t *set_start = reinterpret_cast<const uint8_t *>(set);
        const uint8_t *set_end = set_start + set_len;

        // Check if at least one parsed Data Record is in the Data Set
        bool found = false;
        for (uint32_t rec_id = 0; rec_id < drec_cnt; ++rec_id) {
            const ipx_ipfix_record *rec_info = ipx_msg_ipfix_get_drec(message, rec_id);
            if (rec_info->rec.data < set_start) {
                // The record is in any previous Data Sets -> try next record
                continue;
            }

            if (rec_info->rec.data > set_end) {
                // The record is in any following Data Sets -> stop searching
                break;
            }

            // Found
            found = true;
            break;
        }

        if (found) {
            // Copy the Data Set
            std::memcpy(buffer.get() + new_pos, set, set_len);
            new_pos += set_len;
        } else {
            // Skip the Data Set
            IPX_CTX_DEBUG(plugin_context, "Unknown Template of Data Set (ID %" PRIu16 ")", set_id);
        }
    }

    // Update IPFIX Message header
    assert(new_pos <= msg_size && "Modified IPFIX Message must be the same or smaller!");
    new_hdr->length = htons(uint16_t(new_pos));
    new_hdr->seq_num = htonl(odid_context->sequence_number);
    odid_context->sequence_number += drec_cnt;

    std::fwrite(buffer.get(), new_pos, 1, output_file);
}

/**
 * \brief Remove a Transport Session from all ODID contexts
 *
 * \param[in] session Transport Session to remove
 */
void
IPFIXOutput::remove_session(const struct ipx_session *session)
{
    auto it = odid_contexts.begin();
    while (it != odid_contexts.end()) {
        // Has this session access to write to the file?
        struct odid_context_s &ctx = it->second;
        if (ctx.session != session) {
            // Remove it from colliding sessions, if present
            ctx.colliding_sessions.erase(session);
            it++;
            continue;
        }

        // This is the Transport Session with rights to write to the file with given ODID
        if (ctx.colliding_sessions.empty()) {
            // Only session in the context -> remove the whole context
            it = odid_contexts.erase(it);
            continue;
        }

        /* There are more sessions with the given ODID...
         * The new one will be selected, but it must redefine its (Options) Templates!
         */
        ctx.session = nullptr;
        ctx.colliding_sessions.clear();
        ctx.needs_to_write_templates = true;
        it++;
    }
}

void
IPFIXOutput::on_session_message(ipx_msg_session *message)
{
    ipx_msg_session_event event = ipx_msg_session_get_event(message);
    const ipx_session *session = ipx_msg_session_get_session(message);

    switch (event) {
    case IPX_MSG_SESSION_OPEN:
        // A new Transport Session -> nothing to do
        break;

    case IPX_MSG_SESSION_CLOSE:
        // Free up ODIDs used by this session allowing them to be used again
        remove_session(session);
        break;
    }
}

IPFIXOutput::IPFIXOutput(const Config *config, const ipx_ctx *ctx) : plugin_context(ctx), config(config)
{
    buffer.reset(new uint8_t[UINT16_MAX]);
}

IPFIXOutput::~IPFIXOutput()
{
    close_file();
}
