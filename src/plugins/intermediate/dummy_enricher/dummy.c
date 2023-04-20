/**
 * \file src/plugins/intermediate/asn/asn.c
 * \author Adam Zvara <xzvara01@stud.fit.vutbr.cz>
 * \brief IPv4/IPv6 autonomous system number (ASN) module
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

#include <ipfixcol2.h>

#include "config.h"


/** Instance */
struct instance_data {
    /** Parsed configuration of the instance    */
    struct dummy_config *config;
    /** Modifier instance                       */
    ipx_modifier_t *modifier;
    /** Message builder instance                */
    ipx_msg_builder_t *builder;
    /** Definition of newly appended fields     */
    struct ipx_modifier_field *new_fields;
};

/**
 * \brief Set output buffers with values taken from plugin configuration
 *
 * Output buffers are then used in modifier to modify given record with filled values.
 * \param[in]  rec    Data record from IPFIX message
 * \param[out] output Output buffers
 * \param[in]  data   Callback data
 * \return #IPX_OK on success
 */
int
modifier_callback(const struct fds_drec *rec, struct ipx_modifier_output output[], void *data)
{
    (void) rec;
    struct dummy_config *config = (struct dummy_config *) data;

    // Set output buffers
    size_t out_idx = 0;
    for (uint16_t i = 0; i < config->fields_count; i++) {
        struct new_field *field = &config->fields[i];

        for (uint16_t j = 0; j < field->times; j++) {
            struct ipx_modifier_output *out = &output[out_idx++];

            if (field->type == DUMMY_FIELD_INT) {
                out->length = 4;
                int int_val = htonl(field->value.integer);
                memcpy(out->raw, &int_val, 4);
            } else if (field->type == DUMMY_FIELD_DOUBLE) {
                out->length = 8;
                memcpy(out->raw, &field->value.integer, 8);
            } else {
                out->length = strlen(field->value.string);
                memcpy(out->raw, field->value.string, out->length);
            }
        }

    }
    return IPX_OK;
}

/**
 * \brief Process session message
 *
 * In the event of closing session, information about the particular Transport Session will be removed.
 * \param[in] ctx       Plugin context
 * \param[in] modifier  Modifier component
 * \param[in] msg       Transport session message
 * \return Always #IPX_OK
 */
static int
process_session(ipx_ctx_t *ctx, ipx_modifier_t *modifier, ipx_msg_session_t *msg)
{
    // Only process session close events
    if (ipx_msg_session_get_event(msg) != IPX_MSG_SESSION_CLOSE) {
        ipx_ctx_msg_pass(ctx, ipx_msg_session2base(msg));
        return IPX_OK;
    }

    int rc;
    const struct ipx_session *session = ipx_msg_session_get_session(msg);

    // Always pass the original session message
    ipx_ctx_msg_pass(ctx, ipx_msg_session2base(msg));

    ipx_msg_garbage_t *garbage;
    if ((rc = ipx_modifier_remove_session(modifier, session, &garbage)) == IPX_OK) {
        // Again, possible memory leak
        if (garbage == NULL) {
            IPX_CTX_WARNING(ctx, "A memory allocation failed (%s:%d).", __FILE__, __LINE__);
            return IPX_OK;
        }

        /* Send garbage after session message because other plugins might have references to
           templates linked to that session */
        ipx_ctx_msg_pass(ctx, ipx_msg_garbage2base(garbage));
        return IPX_OK;
    }

    // Possible internal errors
    switch (rc) {
    case IPX_ERR_NOTFOUND:
        IPX_CTX_ERROR(ctx, "Received an event about closing of unknown Transport Session '%s'.",
            session->ident);
        break;
    default:
        IPX_CTX_ERROR(ctx, "ipx_modifier_session_remove() returned an unexpected value (%s:%d, "
            "code: %d).", __FILE__, __LINE__, rc);
        break;
    }

    return IPX_OK;
}

/**
 * \brief Estimate size of new message based on old message
 *
 * \param[in] msg IPFIX message
 * \return Estimated size
 */
static inline int
estimate_new_length(ipx_msg_ipfix_t *msg, struct dummy_config *config)
{
    const struct fds_ipfix_msg_hdr *hdr = (struct fds_ipfix_msg_hdr *) ipx_msg_ipfix_get_packet(msg);
    uint32_t rec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);
    uint32_t msg_size = ntohs(hdr->length);

    if (rec_cnt == 0) {
        return msg_size;
    }

    // Estimate size of new message
    uint32_t new_size = 0;
    for (uint16_t i = 0; i < config->fields_count; i++) {
        struct new_field *field = &config->fields[i];

        if (field->type == DUMMY_FIELD_INT) {
            new_size += 4 * field->times;
        } else if (field->type == DUMMY_FIELD_DOUBLE) {
            new_size += 8 * field->times;
        } else {
            new_size += strlen(field->value.string) * field->times;
        }
    }

    return rec_cnt * (FDS_IPFIX_SET_HDR_LEN + new_size + msg_size / rec_cnt);
}

/**
 * \brief Add new session context from current message to modifier
 *
 * \param[in] ctx       Plugin context
 * \param[in] modifier  Modifier
 * \param[in] msg       IPFIX message
 * \return #IPX_OK on success
 * \return #IPX_ERR_ARG on invalid arguments
 * \return #IPX_ERR_NOMEM on memory allocation error
 */
static int
ipfix_add_session(ipx_ctx_t *ctx, ipx_modifier_t *modifier, ipx_msg_ipfix_t *msg)
{
    int rc;

    // Update modifier session context
    ipx_msg_garbage_t *session_garbage;
    rc = ipx_modifier_add_session(modifier, msg, &session_garbage);
    if (session_garbage) {
        ipx_ctx_msg_pass(ctx, ipx_msg_garbage2base(session_garbage));
    }

    if (rc) {
        switch (rc) {
            case IPX_ERR_ARG:
                IPX_CTX_ERROR(ctx, "Invalid arguments passed to ipx_modifier_add_session (%s:%d)",
                    __FILE__, __LINE__);
                return rc;
            case IPX_ERR_FORMAT:
                // Setting time in history for TCP should be blocked by parser
                assert(false);
            case IPX_ERR_NOMEM:
                IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
                return rc;
            default:
                IPX_CTX_ERROR(ctx, "Unexpected error from ipx_modifier_add_session (%s:%d)",
                    __FILE__, __LINE__);
                return rc;
        }
    }

    return IPX_OK;
}

/**
 * \brief Start building new message
 *
 * \param[in] ctx       Plugin context
 * \param[in] modifier  Modifier
 * \param[in] msg       IPFIX message
 * \return #IPX_OK on success
 * \return #IPX_ERR_ARG on invalid arguments
 * \return #IPX_ERR_NOMEM on memory allocation error
 */
static int
ipfix_start_builder(ipx_ctx_t *ctx, ipx_msg_builder_t *builder,
    const struct fds_ipfix_msg_hdr *hdr, const size_t maxsize)
{
    int rc = ipx_msg_builder_start(builder, hdr, maxsize, 0);

    if (rc) {
        switch(rc) {
            case IPX_ERR_ARG:
                IPX_CTX_ERROR(ctx, "Invalid arguments passed to ipx_modifier_add_session (%s:%d)",
                    __FILE__, __LINE__);
                return rc;
            case IPX_ERR_NOMEM:
                IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
                return rc;
            default:
                IPX_CTX_ERROR(ctx, "Unexpected error from ipx_modifier_add_session (%s:%d)",
                    __FILE__, __LINE__);
                return rc;
        }
    }

    return IPX_OK;
}

/**
 * \brief Add record to new message in builder
 *
 * \param[in] ctx     Plugin context
 * \param[in] builder Message builder
 * \param[in] rec     Data record
 * \return
 */
static int
add_record_to_builder(ipx_ctx_t *ctx, ipx_msg_builder_t *builder, const struct fds_drec *rec)
{
    // Store modified record in builder
    int rc = ipx_msg_builder_add_drec(builder, rec);

    if (rc) {
        switch(rc) {
            case IPX_ERR_DENIED:
                // Exceeded builder limit
                IPX_CTX_ERROR(ctx, "Exceeded message builder limit", __FILE__, __LINE__);
                return rc;
            case IPX_ERR_NOMEM:
                IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
                return rc;
            default:
                IPX_CTX_ERROR(ctx, "Unexpected error from ipx_modifier_add_session (%s:%d)",
                    __FILE__, __LINE__);
                return rc;
        }
    }

    return IPX_OK;
}

/**
 * \brief Process IPFIX message
 *
 * Iterate through all data records in message and modify them, store modified records
 * in new message which is passed instead of original message.
 * \param[in] ctx       Plugin context
 * \param[in] modifier  Modifier component
 * \param[in] builder   Message builder
 * \param[in] msg       IPFIX message
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED on any error
 */
static int
process_ipfix(ipx_ctx_t *ctx, ipx_modifier_t *modifier, ipx_msg_builder_t *builder,
    struct dummy_config *config, ipx_msg_ipfix_t *msg)
{
    int rc;

    // Add session
    rc = ipfix_add_session(ctx, modifier, msg);
    if (rc) {
        return rc;
    }

    // Estimate size of new message
    size_t new_msg_size = estimate_new_length(msg, config);

    // Start builder
    const struct fds_ipfix_msg_hdr *hdr = (struct fds_ipfix_msg_hdr *) ipx_msg_ipfix_get_packet(msg);
    rc = ipfix_start_builder(ctx, builder, hdr, new_msg_size);
    if (rc) {
        return rc;
    }

    // Modify each record in IPFIX message and store modified record in builder
    uint32_t rec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);
    struct ipx_ipfix_record *rec;
    struct fds_drec *modified_rec;
    ipx_msg_garbage_t *ipfix_garbage;
    for (uint32_t i = 0; i < rec_cnt; i++) {
        rec = ipx_msg_ipfix_get_drec(msg, i);

        if (rec->rec.tmplt->type == FDS_TYPE_TEMPLATE_OPTS) {
            // Options template found ... just add it into new message
            if (add_record_to_builder(ctx, builder, &(rec->rec)) != IPX_OK) {
                // Error ... proper message has been already printed
                return rc;
            }
            continue;
        }

        // Modify record
        modified_rec = ipx_modifier_modify(modifier, &(rec->rec), &ipfix_garbage);
        if (ipfix_garbage) {
            ipx_ctx_msg_pass(ctx, ipx_msg_garbage2base(ipfix_garbage));
        }
        if (!modified_rec) {
            // Error ... proper message has been already printed
            return IPX_ERR_DENIED;
        }

        // Add modified record to new message
        rc = add_record_to_builder(ctx, builder, modified_rec);
        if (rc != IPX_OK) {
            // Error ... proper message has been already printed
            return rc;
        }

        free(modified_rec->data);
        free(modified_rec);
    }

    // Create new message with modified records
    const struct ipx_msg_ctx *msg_ctx = ipx_msg_ipfix_get_ctx(msg);
    ipx_msg_ipfix_t *new_msg = ipx_msg_builder_end(builder, ctx, msg_ctx);
    if (!new_msg) {
        return IPX_ERR_DENIED;
    }

    ipx_msg_ipfix_destroy(msg);
    ipx_ctx_msg_pass(ctx, ipx_msg_ipfix2base(new_msg));
    return IPX_OK;
}

// -------------------------------------------------------------------------------------------------

/** Plugin description */
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin type
    .type = IPX_PT_INTERMEDIATE,
    // Plugin identification name
    .name = "dummy_enricher",
    // Brief description of plugin
    .dsc = "Dummy module for adding dummy fields to IPFIX messages",
    // Configuration flags (reserved for future use)
    .flags = 0,
    // Plugin version string (like "1.2.3")
    .version = "2.0.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    .ipx_min = "2.3.0"
};

struct ipx_modifier_field *
create_modifier_fields(struct dummy_config *config, size_t fsize)
{
    struct ipx_modifier_field *fields = calloc(fsize, sizeof(*fields));
    if (!fields) {
        return NULL;
    }

    // Create new fields
    size_t field_idx = 0;
    for (size_t i = 0; i < config->fields_count; i++) {

        for (size_t j = 0; j < config->fields[i].times; j++) {
            fields[field_idx].id = config->fields[i].id;
            fields[field_idx].en = config->fields[i].pen;

            if (config->fields[i].type == DUMMY_FIELD_STRING) {
                fields[field_idx].length = 65535;
            } else if (config->fields[i].type == DUMMY_FIELD_INT) {
                fields[field_idx].length = 4;
            } else {
                fields[field_idx].length = 8;
            }
            field_idx++;
        }
    }

    return fields;
}

int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params)
{
    // Create a private data
    struct instance_data *data = calloc(1, sizeof(*data));
    if (!data) {
        return IPX_ERR_DENIED;
    }

    // Parse XML configuration
    if ((data->config = config_parse(ctx, params)) == NULL) {
        free(data);
        return IPX_ERR_DENIED;
    }

    enum ipx_verb_level verb = ipx_ctx_verb_get(ctx);
    const fds_iemgr_t *iemgr = ipx_ctx_iemgr_get(ctx);
    const char *ident        = ipx_ctx_name_get(ctx);
    size_t fsize = 0;

    // Get amount of new fields
    for (size_t i = 0; i < data->config->fields_count; i++) {
        fsize += data->config->fields[i].times;
    }

    // Create modifier fields based on configuration
    data->new_fields = create_modifier_fields(data->config, fsize);
    if (!data->new_fields) {
        config_destroy(data->config);
        free(data);
        return IPX_ERR_DENIED;
    }

    // Create modifier
    data->modifier = ipx_modifier_create(data->new_fields, fsize, data->config, iemgr, &verb, ident);
    if (!data->modifier) {
        config_destroy(data->config);
        free(data);
        return IPX_ERR_DENIED;
    }

    // Create message builder
    data->builder = ipx_msg_builder_create();
    if (!data->builder) {
        config_destroy(data->config);
        ipx_modifier_destroy(data->modifier);
        free(data);
        return IPX_ERR_DENIED;
    }

    ipx_ctx_private_set(ctx, data);

    // Subscribe to session messages
    const ipx_msg_mask_t new_mask = IPX_MSG_SESSION | IPX_MSG_IPFIX;
    if (ipx_ctx_subscribe(ctx, &new_mask, NULL) != IPX_OK) {
        return IPX_ERR_DENIED;
    }

    // Set adder callback
    ipx_modifier_set_adder_cb(data->modifier, &modifier_callback);

    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg)
{
    (void) ctx; // Suppress warnings
    struct instance_data *data = (struct instance_data *) cfg;

    /* Create garbage message when destroying modifier because other plugins might be
       referencing templates in modifier */

    ipx_msg_garbage_cb cb = (ipx_msg_garbage_cb) &ipx_modifier_destroy;
    ipx_msg_garbage_t *gb_msg = ipx_msg_garbage_create(data->modifier, cb);
    if (gb_msg) {
        ipx_ctx_msg_pass(ctx, ipx_msg_garbage2base(gb_msg));
    } else {
        /* If creating garbage message fails, don't destroy modifier (memory leak) because
           of possible segfault */
        IPX_CTX_WARNING(ctx, "Could not destroy modifier (%s)", ipx_ctx_name_get(ctx));
    }

    ipx_msg_builder_destroy(data->builder);
    free(data->new_fields);
    config_destroy(data->config);
    free(data);
}

int
ipx_plugin_process(ipx_ctx_t *ctx, void *cfg, ipx_msg_t *msg)
{
    struct instance_data *data = (struct instance_data *) cfg;
    int rc;

    // Get message type, call appropriate functions for message type
    enum ipx_msg_type type = ipx_msg_get_type(msg);
    switch (type) {
        case IPX_MSG_SESSION:
            rc = process_session(ctx, data->modifier, ipx_msg_base2session(msg));
            break;
        case IPX_MSG_IPFIX:
            rc = process_ipfix(ctx, data->modifier, data->builder, data->config, ipx_msg_base2ipfix(msg));
            break;
        default:
            // Should not reach
            assert(false);
    }

    if (rc) {
        // Any problem occured when modifying message
        // pass original message
        ipx_ctx_msg_pass(ctx, msg);
        return IPX_OK;
    }

    return IPX_OK;
}