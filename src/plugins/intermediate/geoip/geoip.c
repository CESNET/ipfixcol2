/**
 * \file src/plugins/intermediate/geo/geo.c
 * \author Adam Zvara <xzvara01@stud.fit.vutbr.cz>
 * \brief IPv4/IPv6 autonomous system number (GEO) module
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

/** Definitions of MMDB lookup strings
 *  Best way to look for these defintions is to use mmdbinspect tool
 *  from: https://github.com/maxmind/mmdbinspect
*/
#define GEO_LOOKUP_CONT      "continent", "code",      NULL
#define GEO_LOOKUP_COUNTRY   "country",   "iso_code",  NULL
#define GEO_LOOKUP_LATITUDE  "location",  "latitude",  NULL
#define GEO_LOOKUP_LONGITUDE "location",  "longitude", NULL
#define GEO_LOOKUP_CITY      "city", "names", "en",    NULL

/** Maximum size of appended fields (used for creating new message)
 * 4x double (8B) + 2x country code (2B) = 34 (round up to 50 just in case)
*/
#define GEO_INFO_SIZE 50

/** Instance data structure */
struct instance_data {
    /** Parsed configuration of the instance    */
    struct geo_config *config;
    /** Modifier instance                       */
    ipx_modifier_t *modifier;
    /** Message builder instance                */
    ipx_msg_builder_t *builder;
    /** MaxMindDB database instance             */
    MMDB_s database;
};

/** Direction of GeoIP information      */
enum geo_direction {GSRC, GDST};

/** Diffent types of GeoIP information  */
enum geo_type {
    GSRC_CONT_CODE,
    GDST_CONT_CODE,
    GSRC_COUNTRY_CODE,
    GDST_COUNTRY_CODE,
    GSRC_CITY_NAME,
    GDST_CITY_NAME,
    GSRC_LATITUDE,
    GDST_LATITUDE,
    GSRC_LONGITUDE,
    GDST_LONGITUDE
};

/** Fields defining new possible elements in enriched messages */
struct ipx_modifier_field geo_fields[] = {
    [GSRC_CONT_CODE]    = {.id = 1200, .length = 65535, .en = 8057}, // sourceContinentName
    [GDST_CONT_CODE]    = {.id = 1201, .length = 65535, .en = 8057}, // destinationContinentName
    [GSRC_COUNTRY_CODE] = {.id = 1202, .length = 65535, .en = 8057}, // sourceCountryName
    [GDST_COUNTRY_CODE] = {.id = 1203, .length = 65535, .en = 8057}, // destinationCountryName
    [GSRC_CITY_NAME]    = {.id = 1204, .length = 65535, .en = 8057}, // sourceCityName
    [GDST_CITY_NAME]    = {.id = 1205, .length = 65535, .en = 8057}, // destinationCityName
    [GSRC_LATITUDE]     = {.id = 1206, .length = 8, .en = 8057},     // sourceLatitude
    [GDST_LATITUDE]     = {.id = 1207, .length = 8, .en = 8057},     // destinationLatitude
    [GSRC_LONGITUDE]    = {.id = 1208, .length = 8, .en = 8057},     // sourceLatitude
    [GDST_LONGITUDE]    = {.id = 1209, .length = 8, .en = 8057}      // destinationLatitude
};

/**
 * \brief Auxiliary function for storing double value to output buffer
 *
 * Value is stored in network byte order.
 *
 * \param[in]  data Result of MMDB_get_value function
 * \param[out] out  Output buffer
 */
static inline void
geo_store_double(MMDB_entry_data_s *data, struct ipx_modifier_output *out)
{
     if (data->has_data) {
        // Convert double to network byte order
        double value = data->double_value;
        int64_t tmp_num = htobe64(le64toh(*(int64_t*)&value));
        double  dst_num = *(double*)&tmp_num;
        // Copy data to output buffer
        memcpy(out->raw, &(dst_num), 8);
        out->length = 8;
    }
}

/**
 * \brief Auxiliary function for storing string value to output buffer
 *
 * \param[in]  data Result of MMDB_get_value function
 * \param[out] out  Output buffer
 */
static inline void
geo_store_string(MMDB_entry_data_s *data, struct ipx_modifier_output *out)
{
    if (data->has_data) {
        assert(data->type == MMDB_DATA_TYPE_UTF8_STRING);
        assert(data->data_size != 0);

        out->length = data->data_size;
        memcpy(out->raw, data->utf8_string, data->data_size);
    }
}

/**
 * \brief Fill output buffer with continent code
 *
 * \param[in]  result Result of MaxMindDB lookup
 * \param[out] out    Output buffer
 */
static inline void
geo_continent(MMDB_lookup_result_s *result, struct ipx_modifier_output *out)
{
    // Find country code
    MMDB_entry_data_s data;
    int status = MMDB_get_value(&(result->entry), &data, GEO_LOOKUP_CONT);
    if (status != MMDB_SUCCESS) {
        return;
    }
    // Store continent to output buffer
    geo_store_string(&data, out);
}

/**
 * \brief Fill output buffer with country code
 *
 * \param[in]  result Result of MaxMindDB lookup
 * \param[out] out    Output buffer
 */
static inline void
geo_country(MMDB_lookup_result_s *result, struct ipx_modifier_output *out)
{
    // Find country code
    MMDB_entry_data_s data;
    int status = MMDB_get_value(&(result->entry), &data, GEO_LOOKUP_COUNTRY);
    if (status != MMDB_SUCCESS) {
        return;
    }
    // Store country to output buffer
    geo_store_string(&data, out);
}

/**
 * \brief Fill output buffer with city name
 *
 * \param[in]  result Result of MaxMindDB lookup
 * \param[out] out    Output buffer
 */
static inline void
geo_city(MMDB_lookup_result_s *result, struct ipx_modifier_output *out)
{
    // Find country code
    MMDB_entry_data_s data;
    int status = MMDB_get_value(&(result->entry), &data, GEO_LOOKUP_CITY);
    if (status != MMDB_SUCCESS) {
        return;
    }
    // Store city to output buffer
    geo_store_string(&data, out);
}

/**
 * \brief Fill output buffer with latitude information
 *
 * \param[in]  result Result of MaxMindDB lookup
 * \param[out] out    Output buffer
 */
static inline void
geo_latitude(MMDB_lookup_result_s *result, struct ipx_modifier_output *out)
{
    // Find latitude
    MMDB_entry_data_s data;
    int status = MMDB_get_value(&(result->entry), &data, GEO_LOOKUP_LATITUDE);
    if (status != MMDB_SUCCESS) {
        return;
    }
    // Store latitude to output buffer
    geo_store_double(&data, out);
}

/**
 * \brief Fill output buffer with longitude information
 *
 * \param[in]  result Result of MaxMindDB lookup
 * \param[out] out    Output buffer
 */
static inline void
geo_longitude(MMDB_lookup_result_s *result, struct ipx_modifier_output *out)
{
    // Find longitude
    MMDB_entry_data_s data;
    int status = MMDB_get_value(&(result->entry), &data, GEO_LOOKUP_LONGITUDE);
    if (status != MMDB_SUCCESS) {
        return;
    }
    // Store longitude to output buffer
    geo_store_double(&data, out);
}

/**
 * \brief Get the database entry object
 *
 * \param[in]  db       Initialized MaxMindDB database
 * \param[in]  address  IP address
 * \param[out] result   Result of MaxMindDB lookup
 * \return #IPX_OK on success, #IPX_ERR_DENIED on error, #IPX_ERR_NOTFOUND if address not found
 */
static inline int
get_database_entry(MMDB_s *db, struct sockaddr *address, MMDB_lookup_result_s *result)
{
    int rc = 0;

    // Search for address
    *result = MMDB_lookup_sockaddr(db, address, &rc);
    if (rc) {
        MMDB_strerror(rc);
        return IPX_ERR_DENIED;
    }

    // IP address not found
    if (!result->found_entry) {
        return IPX_ERR_NOTFOUND;
    }

    return IPX_OK;
}

/**
 * \brief Fill output buffers with geographical information
 *
 * \param[in] db        Initialized MaxMindDB database
 * \param[in] output    Output buffers
 * \param[in] address   IP address
 * \param[in] length    Length of IP address
 * \param[in] direction Direction of information (source or destination)
 * \return #IPX_OK on success, #IPX_ERR_ARG on invalid address, #IPX_ERR_DENIED ony MMDB error
 */
static int
get_geo_info(MMDB_s *db, uint8_t *fields, struct ipx_modifier_output output[], uint8_t *address,
    size_t length,  enum geo_direction direction)
{
    int rc = 0;
    MMDB_lookup_result_s result;

    if (length == 4) {
        // IPv4 address
        struct sockaddr_in addr_4 = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = *((uint32_t *)address)
        };
        rc = get_database_entry(db, (struct sockaddr*) &addr_4, &result);
    } else if (length == 16) {
        // IPv6 address
        struct sockaddr_in6 addr_6 = {
            .sin6_family = AF_INET6
        };
        memcpy(&(addr_6.sin6_addr), address, 16);
        rc = get_database_entry(db, (struct sockaddr*) &addr_6, &result);
    } else {
        // Unknown address format
        return IPX_ERR_ARG;
    }

    if (rc != IPX_OK) {
        if (rc == IPX_ERR_NOTFOUND) {
            // Address not found
            return IPX_OK;
        }
        // MMDB error
        return IPX_ERR_DENIED;
    }

    // Found address, fill output buffers
    struct ipx_modifier_output *cont, *country, *city, *latitude, *longitude;

    if (direction == GSRC) {
        cont      = &(output[GSRC_CONT_CODE]);
        country   = &(output[GSRC_COUNTRY_CODE]);
        city      = &(output[GSRC_CITY_NAME]);
        latitude  = &(output[GSRC_LATITUDE]);
        longitude = &(output[GSRC_LONGITUDE]);
    } else {
        cont      = &(output[GDST_CONT_CODE]);
        country   = &(output[GDST_COUNTRY_CODE]);
        city      = &(output[GDST_CITY_NAME]);
        latitude  = &(output[GDST_LATITUDE]);
        longitude = &(output[GDST_LONGITUDE]);
    }

    // Store information to output buffers only if requested in config file
    if (fields[GPARAM_CONT_CODE]) {
        geo_continent(&result, cont);
    }

    if (fields[GPARAM_COUNTRY_CODE]) {
        geo_country(&result, country);
    }

    if (fields[GPARAM_CITY_NAME]) {
        geo_city(&result, city);
    }

    if (fields[GPARAM_LATITUDE]) {
        geo_latitude(&result, latitude);
    }

    if (fields[GPARAM_LONGITUDE]) {
        geo_longitude(&result, longitude);
    }
    return IPX_OK;
}


/**
 * \brief Main function for appending geographical information to IPFIX message
 *
 * \param[in]  rec    Data record from IPFIX message
 * \param[out] output Output buffers
 * \param[in]  data   Callback data
 * \return #IPX_OK on success
 */
int
modifier_geo_callback(const struct fds_drec *rec, struct ipx_modifier_output output[], void *data)
{
    int rc;
    struct instance_data *inst_data = (struct instance_data *) data;
    MMDB_s *db = &(inst_data->database);

    // By default, do not include all fields from database
    for (size_t i = 0; i < sizeof(geo_fields)/sizeof(geo_fields[0]); i++) {
        assert(i / 2 < GPARAM_CNT);
        if (!inst_data->config->fields[i/2]) {
            output[i].length = IPX_MODIFIER_SKIP;
        }
    }

    // Source address
    struct fds_drec_field field;
    if (fds_drec_find((struct fds_drec *)rec, 0, 8, &field) != FDS_EOC) {
        // IPv4
        assert(field.size == 4);
        rc = get_geo_info(db, inst_data->config->fields, output, field.data, 4, GSRC);
    } else if (fds_drec_find((struct fds_drec *)rec, 0, 27, &field) != FDS_EOC) {
        // IPv6
        assert(field.size == 16);
        rc = get_geo_info(db, inst_data->config->fields, output, field.data, 16, GSRC);
    }

    if (rc) {
        return rc;
    }

    // Destination address
    if (fds_drec_find((struct fds_drec *)rec, 0, 12, &field) != FDS_EOC) {
        // IPv4
        assert(field.size == 4);
        rc = get_geo_info(db, inst_data->config->fields, output, field.data, 4, GDST);
    } else if (fds_drec_find((struct fds_drec *)rec, 0, 28, &field) != FDS_EOC) {
        // IPv6
        assert(field.size == 16);
        rc = get_geo_info(db, inst_data->config->fields, output, field.data, 16, GDST);
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

    //for each record * (size of new header + GEO info + size of single record)
    return rec_cnt * (FDS_IPFIX_SET_HDR_LEN + GEO_INFO_SIZE + msg_size / rec_cnt);
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
            // Proper message has been already printed
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
    .name = "geoip",
    // Brief description of plugin
    .dsc = "IPv4/IPv6 geographic information (GeoIP) module",
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


    // Setup GEO database
    if (MMDB_open(data->config->db_path, MMDB_MODE_MMAP, &(data->database)) != MMDB_SUCCESS) {
        config_destroy(data->config);
        free(data);
        return IPX_ERR_DENIED;
    }

    // Create modifier
    enum ipx_verb_level verb = ipx_ctx_verb_get(ctx);
    const fds_iemgr_t *iemgr = ipx_ctx_iemgr_get(ctx);
    const char *ident        = ipx_ctx_name_get(ctx);
    size_t fsize             = sizeof(geo_fields) / sizeof(*geo_fields);

    data->modifier = ipx_modifier_create(geo_fields, fsize, data, iemgr, &verb, ident);
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
        config_destroy(data->config);
        MMDB_close(&(data->database));
        ipx_modifier_destroy(data->modifier);
        ipx_msg_builder_destroy(data->builder);
        free(data);
        return IPX_ERR_DENIED;
    }

    // Set adder callback
    ipx_modifier_set_adder_cb(data->modifier, &modifier_geo_callback);

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