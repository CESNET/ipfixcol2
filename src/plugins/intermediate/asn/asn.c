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
#include <maxminddb.h>

#include "config.h"

#define ASN_LOOKUP_STRING "autonomous_system_number"

/** Instance */
struct instance_data {
    /** Parsed configuration of the instance    */
    struct asn_config *config;
    /** Modifier instance                       */
    ipx_modifier_t *modifier;
    /** Message builder instance                */
    ipx_msg_builder_t *builder;
    /** MaxMindDB database instance             */
    MMDB_s database;
};

/** Type of AS number */
enum asn_type {ASN_SRC, ASN_DST};

/** Fields defining new possible elements in enriched messages */
struct ipx_modifier_field asn_fields[] = {
    [ASN_SRC] = {.id = 16, .length = 4, .en = 0},   // bgpSourceASNumber
    [ASN_DST] = {.id = 17, .length = 4, .en = 0}    // bgpDestinationASNumber
};

/**
 * \brief Get the autonomous system (AS) number from database
 *
 * Function takes IP address, tries to find it in ASN database and return AS number.
 * If number is not found, 0 is returned. If any error occurs, \p err is set to non-zero value.
 * \param[in]  db      ASN database
 * \param[in]  address IP address to search for
 * \param[out] err     MMDB error code
 * \return AS number if successful
 * \return 0 (reserved AS number) if number was not found
 */
static uint32_t
mmdb_lookup(MMDB_s *db, struct sockaddr *address, int *err)
{
    assert(db != NULL && address != NULL && err != NULL);

    int rc;
    *err = MMDB_SUCCESS;
    MMDB_lookup_result_s result;

    // Search for address
    result = MMDB_lookup_sockaddr(db, address, &rc);
    if (rc) {
        *err = rc;
        return 0;
    }

    // IP address not found
    if (!result.found_entry) {
        return 0;
    }

    // Found address, look for AS number
    MMDB_entry_data_s data;
    rc = MMDB_get_value(&(result.entry), &data, ASN_LOOKUP_STRING, NULL);
    if (rc) {
        *err = rc;
        return 0;
    }

    // Address was not found
    if (!data.has_data) {
        return 0;
    }

    return data.uint32;
}

/**
 * \brief Fill output buffer with AS number from ASN databse
 *
 * \param[in] db      ASN database
 * \param[in] address IP address
 * \param[in] length  Length of address (4 or 16)
 * \return #IPX_OK if successful
 * \return #IPX_ERR_ARG if argument was not valid IP address
 * \return #IPX_ERR_DENIED if MMDB error occured
 */
static int
get_asn(MMDB_s *db, struct ipx_modifier_output *out, uint8_t *address, size_t length)
{
    int rc;
    uint32_t asn;
    struct sockaddr *addr;

    if (length == 4) {
        // IPv4 address
        struct sockaddr_in addr_4 = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = *((uint32_t *)address)
        };
        addr = (struct sockaddr *) &addr_4;
    } else if (length == 16) {
        // IPv6 address
        struct sockaddr_in6 addr_6 = {
            .sin6_family = AF_INET6
        };
        memcpy(&(addr_6.sin6_addr), address, 16);
        addr = (struct sockaddr *) &addr_6;
    } else {
        // Unknown address format
        return IPX_ERR_ARG;
    }

    asn = htonl(mmdb_lookup(db, addr, &rc));
    if (rc) {
        // MMDB error
        return IPX_ERR_DENIED;
    }

    if (asn == 0) {
        // Not found
        return IPX_OK;
    }

    int len = 4;
    memcpy(out->raw, &asn, len);
    out->length = len;
    return IPX_OK;
}


/**
 * \brief Set output buffers with autonomous system numbers
 *
 * Output buffers are then used in modifier to modify given record with filled values.
 * Unknown AS numbers are skipped, indicated by negative length in output buffer item.
 * \param[in]  rec    Data record from IPFIX message
 * \param[out] output Output buffers
 * \param[in]  data   Callback data
 * \return #IPX_OK on success
 */
int
modifier_asn_callback(const struct fds_drec *rec, struct ipx_modifier_output output[], void *data)
{
    int rc;
    MMDB_s *db = (MMDB_s *) data;

    // Source address
    struct fds_drec_field field;
    if (fds_drec_find((struct fds_drec *)rec, 0, 8, &field) != FDS_EOC) {
        // IPv4
        assert(field.size == 4);
        rc = get_asn(db, &output[ASN_SRC], field.data, 4);
    } else if (fds_drec_find((struct fds_drec *)rec, 0, 27, &field) != FDS_EOC) {
        // IPv6
        assert(field.size == 16);
        rc = get_asn(db, &output[ASN_SRC], field.data, 16);
    }

    if (rc) {
        return rc;
    }

    // Destination address
    if (fds_drec_find((struct fds_drec *)rec, 0, 12, &field) != FDS_EOC) {
        // IPv4
        assert(field.size == 4);
        rc = get_asn(db, &output[ASN_DST], field.data, 4);
    } else if (fds_drec_find((struct fds_drec *)rec, 0, 28, &field) != FDS_EOC) {
        // IPv6
        assert(field.size == 16);
        rc = get_asn(db, &output[ASN_DST], field.data, 16);
    }

    if (rc) {
        return rc;
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
 * \brief Free modified record
 *
 * \param[in] rec Data record
 */
void
free_modified_record(struct fds_drec *rec)
{
    free(rec->data);
    free(rec);
}

/**
 * \brief Estimate size of new message based on old message
 *
 * Estimation counts with worst case scenario, which is that in new message,
 * for each data record new set is created and both AS numbers are added
 * \param[in] msg IPFIX message
 * \return Estimated size
 */
int
estimate_new_length(ipx_msg_ipfix_t *msg)
{
    const struct fds_ipfix_msg_hdr *hdr = (struct fds_ipfix_msg_hdr *) ipx_msg_ipfix_get_packet(msg);
    uint32_t rec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);
    uint32_t msg_size = ntohs(hdr->length);

    if (rec_cnt == 0) {
        return msg_size;
    }

    //for each record * (size of new header + 2x ASN + size of single record)
    return rec_cnt * (FDS_IPFIX_SET_HDR_LEN + 8 + msg_size / rec_cnt);
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
int
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
int
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
    ipx_msg_ipfix_t *msg)
{
    int rc;

    // Add session
    rc = ipfix_add_session(ctx, modifier, msg);
    if (rc) {
        return rc;
    }

    // Estimate size of new message
    size_t new_msg_size = estimate_new_length(msg);

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

        // Modify record
        modified_rec = ipx_modifier_modify(modifier, &(rec->rec), &ipfix_garbage);
        if (ipfix_garbage) {
            ipx_ctx_msg_pass(ctx, ipx_msg_garbage2base(ipfix_garbage));
        }
        if (!modified_rec) {
            // Proper message has been already printed
            return rc;
        }

        // Store modified record in builder
        rc = ipx_msg_builder_add_drec(builder, modified_rec);

        if (rc) {
            free_modified_record(modified_rec);
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

        free_modified_record(modified_rec);
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
    .name = "asn",
    // Brief description of plugin
    .dsc = "IPv4/IPv6 autonomous system number (ASN) module",
    // Configuration flags (reserved for future use)
    .flags = 0,
    // Plugin version string (like "1.2.3")
    .version = "2.0.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    .ipx_min = "2.3.0"
};

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

    // Setup ASN database
    if (MMDB_open(data->config->db_path, MMDB_MODE_MMAP, &(data->database)) != MMDB_SUCCESS) {
        config_destroy(data->config);
        free(data);
        return IPX_ERR_DENIED;
    }

    // Create modifier
    enum ipx_verb_level verb = ipx_ctx_verb_get(ctx);
    const fds_iemgr_t *iemgr = ipx_ctx_iemgr_get(ctx);
    const char *ident        = ipx_ctx_name_get(ctx);
    size_t fsize             = sizeof(asn_fields) / sizeof(*asn_fields);

    data->modifier = ipx_modifier_create(asn_fields, fsize, &(data->database), iemgr, &verb, ident);
    if (!data->modifier) {
        config_destroy(data->config);
        MMDB_close(&(data->database));
        free(data);
        return IPX_ERR_DENIED;
    }

    // Create message builder
    data->builder = ipx_msg_builder_create();
    if (!data->builder) {
        config_destroy(data->config);
        MMDB_close(&(data->database));
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
    ipx_modifier_set_adder_cb(data->modifier, &modifier_asn_callback);

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
    config_destroy(data->config);
    MMDB_close(&(data->database));
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
            rc = process_ipfix(ctx, data->modifier, data->builder, ipx_msg_base2ipfix(msg));
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