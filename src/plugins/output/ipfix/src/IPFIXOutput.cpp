/**
 * \file src/plugins/output/ipfix/src/IPFIXOutput.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
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
#include <memory>
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
    if (config->window_size == 0 && output_file != NULL) {
        return false;
    }

    uint32_t time_difference = current_time - file_start_time;
    if (time_difference >= config->window_size) {
        return true;
    } else {
        return false;
    }
}

/**
 * @brief Create a new output file
 * @param[in] current_time Current export time
 * @throw runtime_error if the file cannot be created
 */
void
IPFIXOutput::new_file(std::time_t current_time)
{
    // Get the timestamp of the file to create
    if (config->align_windows) {
        // Round down to the nearest multiple of window size
        current_time = (current_time / config->window_size) * config->window_size;
    }
    file_start_time = current_time;

    std::tm ts_now;
    std::tm *ts_res = config->use_localtime
        ? localtime_r(&current_time, &ts_now)
        : gmtime_r(&current_time, &ts_now);
    if (ts_res == NULL) {
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
        odid_pair.second.needs_to_write_templates = true;
    }

    IPX_CTX_INFO(plugin_context, "New output file created: %s", filename);
}

/**
 * \brief Writes bytes to the output
 *
 * \param[in] bytes       The bytes
 * \param[in] bytes_count The bytes count
 */
void
IPFIXOutput::write_bytes(const void *bytes, size_t bytes_count)
{
    size_t bytes_written = std::fwrite(bytes, sizeof (char), bytes_count, output_file);
    if (bytes_written != bytes_count) {
        std::runtime_error("Error writing to output file: only " + std::to_string(bytes_written)
            + " out of " + std::to_string(bytes_count) + " bytes were written!");
    }
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

    output_file = NULL;
    IPX_CTX_INFO(plugin_context, "Closed output file", '\0');
}

/**
 * \brief Iterates over Template IDs and Options Template IDs stored in ODID context
 *   and removes those that are not found in the provided Templates snapshot
 *
 * \param[in] odid_context       The odid context
 * \param[in] templates_snapshot The templates snapshot
 */
void
IPFIXOutput::remove_dead_templates(odid_context_s *odid_context, const fds_tsnapshot_t *templates_snapshot)
{
    // Delete templates that are not found in the snapshot
    auto templates_iter = odid_context->templates_seen.begin();
    while (templates_iter != odid_context->templates_seen.end()) {
        if (fds_tsnapshot_template_get(templates_snapshot, *templates_iter) == NULL) {
            templates_iter = odid_context->templates_seen.erase(templates_iter);
        } else {
            templates_iter++;
        }
    }

    // Delete options templates that are not found in the snapshot
    auto options_templates_iter = odid_context->options_templates_seen.begin();
    while (options_templates_iter != odid_context->options_templates_seen.end()) {
        if (fds_tsnapshot_template_get(templates_snapshot, *options_templates_iter) == NULL) {
            options_templates_iter = odid_context->options_templates_seen.erase(options_templates_iter);
        } else {
            options_templates_iter++;
        }
    }
}

/**
 * \brief      Writes a single template set to the output file
 *
 * \param[in]  set_id              The set id of the template set (2 or 3)
 * \param[in]  templates_snapshot  The templates snapshot
 * \param[in]  template_ids        Iterator to the start of template ids to go through
 * \param[in]  template_ids_end    The end iterator
 * \param[in]  size_limit          Maximum size of the template set, including the header
 * \param[out] out_set_length      Number of bytes written
 *
 * \warning    The function assumes that templates that are no longer available in the templates
 *             snapshot are already removed from the set of template IDs.
 * \return     Number of templates written
 */
int
IPFIXOutput::write_template_set(uint16_t set_id, const fds_tsnapshot_t *templates_snapshot,
     std::set<uint16_t>::iterator template_ids, std::set<uint16_t>::iterator template_ids_end,
     unsigned size_limit, unsigned *out_set_length)
{
    assert(set_id == FDS_IPFIX_SET_TMPLT || set_id == FDS_IPFIX_SET_OPTS_TMPLT);

    // Check if there are any template ids to be written
    if (template_ids == template_ids_end) {
        *out_set_length = 0;
        return 0;
    }

    // Check if we can at least fit header and one template
    const fds_template *first_template = fds_tsnapshot_template_get(templates_snapshot, *template_ids);
    assert(first_template != NULL); // Dead templates should have been removed at this point
    if (sizeof(fds_ipfix_set_hdr) + first_template->raw.length > size_limit) {
        *out_set_length = 0;
        return 0;
    }

    unsigned set_length = 0;

    // Set header
    fds_ipfix_set_hdr set_header;
    set_header.flowset_id = htons(set_id);
    set_header.length = 0; // Unknown at the moment
    set_length += sizeof(set_header);
    // Skip set header for now because the length field is unknown
    long int set_header_start = std::ftell(output_file);
    write_bytes(&set_header, sizeof (set_header));

    // Write raw templates
    int templates_written = 0;
    for (; template_ids != template_ids_end; template_ids++) {
        const fds_template *template_ = fds_tsnapshot_template_get(templates_snapshot, *template_ids);
        assert(template_ != NULL); // Dead templates should have been removed at this point
        assert((set_id == FDS_IPFIX_SET_TMPLT && template_->type == FDS_TYPE_TEMPLATE)
            || (set_id == FDS_IPFIX_SET_OPTS_TMPLT && template_->type == FDS_TYPE_TEMPLATE_OPTS));
        if (size_limit - set_length < template_->raw.length) {
            break;
        }
        write_bytes(template_->raw.data, template_->raw.length);
        set_length += template_->raw.length;
        templates_written++;

        IPX_CTX_DEBUG(plugin_context, "Wrote template with ID %" PRIu16 " to a Template Set.",
            *template_ids);
    }

    // Compute set length now that all the templates are written and finish the header
    assert(set_length > 0 && set_length <= UINT16_MAX);
    set_header.length = htons(set_length);
    std::fseek(output_file, set_header_start, SEEK_SET);
    write_bytes(&set_header, sizeof (set_header));
    std::fseek(output_file, 0, SEEK_END);

    IPX_CTX_DEBUG(plugin_context, "Wrote a template set of %d templates consisting of %d bytes",
        templates_written, set_length);
    *out_set_length = set_length;
    return templates_written;
}

/**
 * \brief Writes all templates stored in the ODID context to the output file.
 *
 * This function should be called after a new output file is created and templates present in the
 * previous files have to be provided.
 *
 * \param[in]  odid_context        The odid context
 * \param[in]  templates_snapshot  The templates snapshot
 * \param[in]  export_time         The export time
 * \param[in]  sequence_number     The sequence number
 */
void
IPFIXOutput::write_templates(odid_context_s *odid_context, const fds_tsnapshot_t *templates_snapshot,
                          uint32_t export_time, uint32_t sequence_number)
{
    unsigned message_size_limit = 1400;

    std::set<uint16_t>::iterator template_ids_iter = odid_context->templates_seen.begin();
    std::set<uint16_t>::iterator template_ids_end = odid_context->templates_seen.end();
    uint16_t set_id = FDS_IPFIX_SET_TMPLT;

    // If there are no regular templates switch to options templates
    if (template_ids_iter == template_ids_end) {
        set_id = FDS_IPFIX_SET_OPTS_TMPLT;
        template_ids_iter = odid_context->options_templates_seen.begin();
        template_ids_end = odid_context->options_templates_seen.end();
    }

    while (template_ids_iter != template_ids_end) {
        unsigned message_length = 0;

        // Message header
        fds_ipfix_msg_hdr message_header;
        message_header.version = htons(FDS_IPFIX_VERSION);
        message_header.export_time = htonl(export_time);
        message_header.odid = htonl(odid_context->odid);
        message_header.seq_num = htonl(sequence_number); // Templates don't increase sequence numbers
        message_header.length = 0; // Unknown at the moment
        message_length += sizeof (message_header);

        // Skip message header for now because the length field is unknown
        long int message_header_start = std::ftell(output_file);
        write_bytes(&message_header, sizeof (message_header));

        // Write templates
        int templates_written;
        int templates_written_total = 0;
        unsigned set_length;
        do {
            templates_written = write_template_set(set_id, templates_snapshot, template_ids_iter,
                template_ids_end, message_size_limit - message_length, &set_length);
            templates_written_total += templates_written;
            if (templates_written_total == 0) {
                // We can't even fit the first template into the message
                IPX_CTX_WARNING(plugin_context, "[ODID: %u] Template with ID %" PRIu16 " is bigger "
                    "than the message size limit! Skipping...",
                    odid_context->odid, *template_ids_iter);
                templates_written = 1;
                set_length = 0;
            }

            message_length += set_length;
            std::advance(template_ids_iter, templates_written);

            if (template_ids_iter == template_ids_end) {
                if (set_id == FDS_IPFIX_SET_TMPLT) {
                    // Switch to options templates if all the regular templates are written
                    set_id = FDS_IPFIX_SET_OPTS_TMPLT;
                    template_ids_iter = odid_context->options_templates_seen.begin();
                    template_ids_end = odid_context->options_templates_seen.end();
                } else {
                    break; // All templates are written
                }
            }
        } while (templates_written > 0); // We can't fit any more template sets into the size limit

        // Finish message header
        assert(message_length > sizeof (message_header) && message_length <= UINT16_MAX);
        message_header.length = htons(message_length);
        std::fseek(output_file, message_header_start, SEEK_SET);
        write_bytes(&message_header, sizeof (message_header));
        std::fseek(output_file, 0, SEEK_END);
    }

    odid_context->needs_to_write_templates = false;
    IPX_CTX_DEBUG(plugin_context, "Wrote templates for ODID %" PRIu32 " (%zu Templates, "
        "%zu Options Templates)", odid_context->odid, odid_context->templates_seen.size(),
        odid_context->options_templates_seen.size());
}

/**
 * \brief Processes an incoming IPFIX message from the collector
 * \param[in] message  The IPFIX message
 * \throws runtime_error if a new file cannot be created
 */
void
IPFIXOutput::on_ipfix_message(ipx_msg_ipfix *message)
{
    const ipx_msg_ctx *message_ipx_ctx = ipx_msg_ipfix_get_ctx(message);
    const fds_ipfix_msg_hdr *original_message_header =
        (fds_ipfix_msg_hdr *)(ipx_msg_ipfix_get_packet(message));
    uint32_t export_time = ntohl(original_message_header->export_time);
    uint32_t odid = ntohl(original_message_header->odid);
    uint32_t seq_num = ntohl(original_message_header->seq_num);

    std::time_t current_time;
    if (!config->split_on_export_time) {
        current_time = std::time(NULL);
    } else {
        // Warning: assigning uint32_t to time_t
        assert(current_time >= 0 && sizeof(current_time) >= sizeof(export_time));
        current_time = export_time;
    }

    // Start a new file if needed
    if (should_start_new_file(current_time)) {
        close_file();
        new_file(current_time);
    }

    // Find context corresponding to the ODID
    odid_context_s *odid_context = &odid_contexts[odid];
    if (odid_context->session != NULL) {
        /* Context for the ODID was found, we have to check if the session matches
         * because there may different sessions using the same ODID.
         * This is not supported because there is no way to differentiate them in
         * the output file as there is no session information.
         * Warning:
         *     Sessions are compared based on pointers for simplicity !!!
         *     We can do this because the pointer doesn't change while the session is open
         */
        if (odid_context->session != message_ipx_ctx->session) {
            /* More than one session is using the same ODID, in that case we only let
             * the messages from the first session using the ODID through and skip the others.
             */
            const auto it = odid_context->colliding_sessions.find(message_ipx_ctx->session);
            if (it != odid_context->colliding_sessions.end()) {
                // We only want to print the warning message once
                IPX_CTX_WARNING(plugin_context, "[ODID: %u] ODID collision detected! "
                    "Messages will be skipped.", odid_context->odid);
                odid_context->colliding_sessions.insert(message_ipx_ctx->session);
            }
            return;
        }
    } else {
        // Context for the ODID was not found in the map and a new one was created
        odid_context->session = message_ipx_ctx->session;
        odid_context->needs_to_write_templates = false;
        odid_context->odid = odid;
        if (config->skip_unknown_datasets) {
            odid_context->sequence_number = seq_num;
        }
    }

    // We need a templates snapshot from a data record, so at least one is needed!
    const uint32_t data_records_count = ipx_msg_ipfix_get_drec_cnt(message);
    const fds_tsnapshot_t *templates_snapshot = NULL;
    if (data_records_count > 0) {
        ipx_ipfix_record *data_record = ipx_msg_ipfix_get_drec(message, 0);
        templates_snapshot = data_record->rec.snap;
    }

    // Write templates if this is the first time this ODID context is being written to the file
    if (templates_snapshot != NULL && odid_context->needs_to_write_templates) {
        uint32_t new_sn = (config->skip_unknown_datasets) ? odid_context->sequence_number : seq_num;
        remove_dead_templates(odid_context, templates_snapshot);
        write_templates(odid_context, templates_snapshot, export_time, new_sn);
    }

    // Message header, copy the original
    unsigned message_length = 0;
    fds_ipfix_msg_hdr message_header;
    std::memcpy(&message_header, original_message_header, sizeof(message_header));

    // Skip message header for now because the length field is unknown
    long int message_header_start = std::ftell(output_file);
    write_bytes(&message_header, sizeof (message_header));
    message_length += sizeof(message_header);

    // Iterate over sets, and write them to the output file.
    // If it's a Template Set store the Template IDs we've seen.
    // If it's a Data Set and we don't know the ID optionally ignore it (if the option is enabled)
    ipx_ipfix_set *sets;
    size_t sets_count;
    ipx_msg_ipfix_get_sets(message, &sets, &sets_count);
    for (size_t set_index = 0; set_index < sets_count; set_index++) {
        // We are only interested in (Options) Template Set and Data Sets
        uint16_t set_id = ntohs(sets[set_index].ptr->flowset_id);
        uint16_t set_length = ntohs(sets[set_index].ptr->length);

        if (set_id == FDS_IPFIX_SET_TMPLT || set_id == FDS_IPFIX_SET_OPTS_TMPLT) {
            // Template or Options Template Set, write it as it is and store the Template IDs
            fds_tset_iter template_iter;
            fds_tset_iter_init(&template_iter, sets[set_index].ptr);

            int return_code;
            while ((return_code = fds_tset_iter_next(&template_iter)) == FDS_OK) {
                if (template_iter.field_cnt == 0) {
                    // Skip (Options) Template Withdrawal
                    uint16_t template_id = ntohs(template_iter.ptr.wdrl_trec->template_id);
                    IPX_CTX_DEBUG(plugin_context, "Seen Template Withdrawal %" PRIu16, template_id);
                    continue;
                }

                if (set_id == FDS_IPFIX_SET_TMPLT) {
                    uint16_t template_id = ntohs(template_iter.ptr.trec->template_id);
                    odid_context->templates_seen.insert(template_id);
                    IPX_CTX_DEBUG(plugin_context, "Seen Template ID %" PRIu16, template_id);
                } else if (set_id == FDS_IPFIX_SET_OPTS_TMPLT) {
                    uint16_t template_id = ntohs(template_iter.ptr.opts_trec->template_id);
                    odid_context->options_templates_seen.insert(template_id);
                    IPX_CTX_DEBUG(plugin_context, "Seen Options Template ID %" PRIu16, template_id);
                }
            }
            if (return_code != FDS_EOC) {
                const char *error_message = fds_tset_iter_err(&template_iter);
                throw std::runtime_error("Error iterating over (Options) Template set: "
                    + std::string(error_message));
            }

            write_bytes(sets[set_index].ptr, set_length);
            message_length += set_length;

        } else if (set_id >= FDS_IPFIX_SET_MIN_DSET) {
            // Data Set
            if (!config->skip_unknown_datasets) {
                // Always add the Set (even if the particular Template is unknown)
                write_bytes(sets[set_index].ptr, set_length);
                message_length += set_length;
                continue;
            }

            const uint8_t *set_start = (const uint8_t *) sets[set_index].ptr;
            const uint8_t *set_end = set_start + set_length;

            // Check if at least one parsed Data Record is in the Data Set
            bool found = false;
            for (uint32_t rec_id = 0; rec_id < data_records_count; ++rec_id) {
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
                write_bytes(sets[set_index].ptr, set_length);
                message_length += set_length;
            } else {
                IPX_CTX_DEBUG(plugin_context, "Unknown template ID for dataset (%d)", set_id);
            }
        } else {
            assert(!"Unhandled Set ID");
        }
    }

    // Finish message header
    assert(message_length >= sizeof(message_header) && message_length <= UINT16_MAX);
    assert(message_length <= ntohs(message_header.length));

    message_header.length = htons(message_length);
    if (config->skip_unknown_datasets) {
        // Update sequence number
        message_header.seq_num = htonl(odid_context->sequence_number);
        odid_context->sequence_number += data_records_count;
    }
    std::fseek(output_file, message_header_start, SEEK_SET);
    write_bytes(&message_header, sizeof (message_header));
    std::fseek(output_file, 0, SEEK_END);
}

void
IPFIXOutput::on_session_message(ipx_msg_session *message)
{
    ipx_msg_session_event event = ipx_msg_session_get_event(message);
    const ipx_session *session = ipx_msg_session_get_session(message);

    switch (event) {
    case IPX_MSG_SESSION_OPEN:
        // Do nothing
        break;

    case IPX_MSG_SESSION_CLOSE:
        // Free up ODIDs used by this session allowing them to be used again
        auto odid_context_iter = odid_contexts.begin();
        while (odid_context_iter != odid_contexts.end()) {
            // Warning: comparison of sessions based on pointers
            if (odid_context_iter->second.session == session) {
                // FIXME: Insert all (Options) Templates Withdrawal into the file and wait for new file?
                odid_context_iter = odid_contexts.erase(odid_context_iter);
            } else {
                odid_context_iter++;
            }
        }
        break;
    }
}

IPFIXOutput::IPFIXOutput(const Config *config, const ipx_ctx *ctx) : plugin_context(ctx), config(config)
{
}

IPFIXOutput::~IPFIXOutput()
{
    close_file();
}