#include <ipfixcol2.h>

// #include "config.h"

/** Instance */
struct instance_data {
    // /** Parsed configuration of the instance    */
    // struct asn_config *config;
    /** Modifier instance                       */
    ipx_modifier_t *modifier;
    /** Message builder instance                */
    ipx_msg_builder_t *builder;

    uint64_t unix_time_ms;
    uint64_t counter;
};


/** Fields defining new possible elements in enriched messages */
struct ipx_modifier_field uuid_field = {.id = 1300, .length = 16, .en = 8057}; // cesnet:flowUuid

static void fill_uuid(uint64_t unix_ts_ms, uint64_t counter, uint32_t rand, uint8_t *out_data) {
    /**
     *    0                   1                   2                   3
     *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |                          unix_ts_ms                           |
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |          unix_ts_ms           |  ver  |        counter        |
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |var|                        counter                            |
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |                             rand                              |
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */

    uint32_t *u32 = (uint32_t *)out_data;
    u32[0] = htobe32(unix_ts_ms >> 16); // Top 32 bits of timestamp
    u32[1] = htobe32(
        ((unix_ts_ms & 0xFFFF) << 16) | // Bottom 17 bits of timestamp
        (0x7UL << 12) | // Version, constant 7
        (counter >> 30) & 0xFFF // Top 12 bits of counter
    );
    u32[2] = htobe32(
        (0x2UL << 30) | // Constant 2
        ((counter >> 12) & 0x3FFF) // Bottom 30 bits of counter
    );
    u32[3] = htobe32(rand & 0xFFFFFFFF);
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
modifier_callback(const struct fds_drec *rec, struct ipx_modifier_output output[], void *data)
{
    struct instance_data *instance = (struct instance_data *)data;

    uint32_t r = (uint32_t)rand();
    fill_uuid(instance->unix_time_ms, instance->counter, r, output->raw);
    output->length = 16;
    instance->counter++;

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
 * Estimation counts with worst case scenario, which is that in new message,
 * for each data record new set is created and both AS numbers are added
 * \param[in] msg IPFIX message
 * \return Estimated size
 */
static inline int
estimate_new_length(ipx_msg_ipfix_t *msg)
{
    const struct fds_ipfix_msg_hdr *hdr = (struct fds_ipfix_msg_hdr *) ipx_msg_ipfix_get_packet(msg);
    uint32_t rec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);
    uint32_t msg_size = ntohs(hdr->length);

    if (rec_cnt == 0) {
        return msg_size;
    }

    //for each record * (size of new header + UUID + size of single record)
    return rec_cnt * (FDS_IPFIX_SET_HDR_LEN + 16 + msg_size / rec_cnt);
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
process_ipfix(ipx_ctx_t *ctx, struct instance_data *instance, ipx_modifier_t *modifier, ipx_msg_builder_t *builder,
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

    // Set up values for callback
    uint64_t now = (uint64_t)time(NULL);

    if (now != instance->unix_time_ms) {
        instance->counter = 0;
        instance->unix_time_ms = now;
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
    .name = "uuid",
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
    // if ((data->config = config_parse(ctx, params)) == NULL) {
    //     free(data);
    //     return IPX_ERR_DENIED;
    // }

    // Create modifier
    enum ipx_verb_level verb = ipx_ctx_verb_get(ctx);
    const fds_iemgr_t *iemgr = ipx_ctx_iemgr_get(ctx);
    const char *ident        = ipx_ctx_name_get(ctx);

    data->modifier = ipx_modifier_create(&uuid_field, 1, &data, iemgr, &verb, ident);
    if (!data->modifier) {
        // config_destroy(data->config);
        free(data);
        return IPX_ERR_DENIED;
    }

    // Create message builder
    data->builder = ipx_msg_builder_create();
    if (!data->builder) {
        // config_destroy(data->config);
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
    // config_destroy(data->config);
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
            rc = process_ipfix(ctx, data, data->modifier, data->builder, ipx_msg_base2ipfix(msg));
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
