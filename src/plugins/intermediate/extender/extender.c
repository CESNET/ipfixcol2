/**
 * \file src/plugins/intermediate/extender/extender.c
 * \author Jimmy Björklund <jimmy@lolo.company>
 * \brief The extender plugin implementation
 * \date 2026
 */

#include <libfds.h>
#include <ipfixcol2.h>
#include <stdlib.h>
#include <arpa/inet.h> // ntohs
#include <endian.h> // be64toh
#include <assert.h>
#include <time.h> // timespec
#include <stdbool.h>
#include <string.h>

#include <stdio.h>

#include "config.h"
#include "msg_builder.h"

// Define VRFName IE ID if not available
#define IE_VRF_NAME 236

IPX_API struct ipx_plugin_info ipx_plugin_info = {
    .type = IPX_PT_INTERMEDIATE,
    .name = "extender",
    .dsc = "Data record extender plugin",
    .flags = 0,
    .version = "0.0.1",
    .ipx_min = "2.0.0"
};

// Linked list to map old template IDs to new ones
struct template_node {
    uint16_t old_id;
    uint16_t new_id;
    struct fds_template *new_tmplt;
    uint8_t *raw_buffer;
    struct template_node *next;
};

struct plugin_ctx {
    struct config *config;    
    ipx_ctx_t *ipx_ctx;
    struct template_node *templates;
    uint16_t next_template_id;
};


struct plugin_ctx *
create_plugin_ctx()
{
    struct plugin_ctx *pctx = calloc(1, sizeof(struct plugin_ctx));
    if (!pctx) {
        return NULL;
    }
    // Start allocating new template IDs from 40000 to avoid conflicts with standard IDs
    pctx->next_template_id = 40000;
    return pctx;
}

void
destroy_plugin_ctx(struct plugin_ctx *pctx)
{
    if (!pctx) {
        return;
    }
    
    // Free template cache
    struct template_node *node = pctx->templates;
    while (node) {
        struct template_node *next = node->next;
        if (node->new_tmplt) {
            fds_template_destroy(node->new_tmplt);
        }
        if (node->raw_buffer) {
            free(node->raw_buffer);
        }
        free(node);
        node = next;
    }
    
    // Free filters explicitly
    printf("Destroying plugin context and freeing filters\n");
    if (pctx->config) {
        config_destroy(pctx->config);
    }    
    free(pctx);
}

static inline bool
record_belongs_to_set(struct fds_ipfix_set_hdr *set, struct fds_drec *record)
{
    uint8_t *set_begin = (uint8_t *) set;
    uint8_t *set_end = set_begin + ntohs(set->length);
    uint8_t *record_begin = record->data;

    return record_begin >= set_begin && record_begin < set_end;
}


static uint16_t size_of_data_type(enum fds_iemgr_element_type data_type)
{
    switch(data_type) {
        case FDS_ET_BOOLEAN:
        case FDS_ET_UNSIGNED_8:
        case FDS_ET_SIGNED_8:
            return 1;
        case FDS_ET_UNSIGNED_16:
        case FDS_ET_SIGNED_16:
            return 2;
        case FDS_ET_UNSIGNED_32:
        case FDS_ET_SIGNED_32:
        case FDS_ET_FLOAT_32:
            return 4;
        case FDS_ET_UNSIGNED_64:
        case FDS_ET_SIGNED_64:
        case FDS_ET_FLOAT_64:
            return 8;
        case FDS_ET_IPV4_ADDRESS:
            return 4;
        case FDS_ET_IPV6_ADDRESS:
            return 16;
        case FDS_ET_STRING:
        case FDS_ET_OCTET_ARRAY:
            return UINT16_MAX; // Variable length
        default:    
            return 0; // Unknown size
    }
}

static size_t get_max_len(const config_ids_t* id)
{
    size_t max_len = 0;
    for( int i =0; i < id->values_count; i++) {
        if (id->values[i].value) {
            size_t len = strlen(id->values[i].value);
            if( len > max_len ) {
                max_len = len;
            }
        }
    }
    return max_len;
}

int ipx_plugin_init(ipx_ctx_t *ipx_ctx, const char *params)
{
    // Create the plugin context
    struct plugin_ctx *pctx = create_plugin_ctx();
    if (!pctx) {
        return IPX_ERR_DENIED;
    }

    pctx->ipx_ctx = ipx_ctx;

    // Parse config
    pctx->config = config_parse(ipx_ctx, params);
    if (!pctx->config) {
        destroy_plugin_ctx(pctx);
        return IPX_ERR_DENIED;
    }

    // Create the opts
    for  (int i = 0; i < pctx->config->ids_count; i++) {
        for ( int v =0; v < pctx->config->ids[i].values_count; v++) {
            config_ids_t* id = &pctx->config->ids[i];
            config_value_t* value = &id->values[v];
            printf("Config ID %s Value %s Expr %s\n", 
                   id->name,
                   value->value,
                   value->expr);
            int rc = fds_ipfix_filter_create(&value->filter, ipx_ctx_iemgr_get(ipx_ctx), value->expr);
            if (rc != FDS_OK) {
                const char *error = fds_ipfix_filter_get_error(value->filter);
                IPX_CTX_ERROR(ipx_ctx, "Error creating filter: %s", error);
                destroy_plugin_ctx(pctx);
                return IPX_ERR_DENIED;
            }

            // from the id lookup the iana template id and data length
            const struct fds_iemgr_elem * elem = fds_iemgr_elem_find_name(ipx_ctx_iemgr_get(ipx_ctx), id->name);
            if (!elem) {
                IPX_CTX_ERROR(ipx_ctx, "Unknown ID (make sure case is correct): %s", id->name);
                destroy_plugin_ctx(pctx);
                return IPX_ERR_DENIED;
            }
            id->id = elem->id;
            id->data_type = elem->data_type;
        }
    }
    size_t tmp_len = 0;
    for (int i = 0; i < pctx->config->ids_count; i++) {        
        const uint16_t size = size_of_data_type(pctx->config->ids[i].data_type);
        if( size == UINT16_MAX ) {
            size_t id_len = get_max_len(&pctx->config->ids[i]);
            if (id_len < 255) 
                id_len += 1;
            else 
                id_len += 3; // 255 + 2 bytes len        
            tmp_len += id_len;
        } else {
            tmp_len += size;
        }
    }
    printf("Maximum extension length per record: %zu bytes\n", tmp_len);
    pctx->config->max_extension_len = tmp_len;
    ipx_ctx_private_set(ipx_ctx, pctx);
    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ipx_ctx, void *data)
{
    (void) ipx_ctx;
    destroy_plugin_ctx(data);
}

    // Helper to find or create an extended template
struct fds_template *
get_or_create_extended_template(struct plugin_ctx *pctx, 
                                const struct fds_template *old_tmplt, 
                                msg_builder_s *builder,
                                const config_ids_t *extension,
                                int extension_count)
{
    // Search cache
    for (struct template_node *node = pctx->templates; node != NULL; node = node->next) {
        if (node->old_id == old_tmplt->id) {
            return node->new_tmplt;
        }
    }

    if (old_tmplt->type == FDS_TYPE_TEMPLATE_OPTS) {
        IPX_CTX_WARNING(pctx->ipx_ctx, "Skipping extension of Options Template ID %u", old_tmplt->id);
        return NULL;
    }

    // Create new template
    uint16_t old_len = old_tmplt->raw.length;
    uint16_t new_len = old_len + (4 * extension_count); // Add IE ID (2B) + Length (2B)
    
    // Debug logging
    // Ensure we are reading 2 bytes at offset 2 for Count
    if (old_len < 4) {
        IPX_CTX_ERROR(pctx->ipx_ctx, "Template %u too short (%u bytes)", old_tmplt->id, old_len);
        return NULL;
    }

    uint8_t *buffer = calloc(1, new_len);
    if (!buffer) {
        return NULL;
    }
    
    // Copy existing template record
    memcpy(buffer, old_tmplt->raw.data, old_len);
    
    uint16_t old_count_n = *(uint16_t *)(old_tmplt->raw.data + 2);
    uint16_t old_count = ntohs(old_count_n);

    // Update Template ID (first 2 bytes)
    uint16_t new_id = pctx->next_template_id++;
    *(uint16_t *)buffer = htons(new_id);
    
    // Update Field Count (next 2 bytes)
    // We modify the buffer we just copied to
    *(uint16_t *)(buffer + 2) = htons(old_count + extension_count);
    
    printf("Cloning Template %u -> %u. Old Count: %u, New Count: %u. Len: %u -> %u\n", 
           old_tmplt->id, new_id, old_count, old_count + extension_count, old_len, new_len);

    uint8_t *ptr = buffer + old_len;
    for( int i = 0; i < extension_count; i++) {
        *(uint16_t *)ptr = htons(extension[i].id);    
        *(uint16_t *)(ptr + 2) = htons(size_of_data_type(extension[i].data_type)); // TOTO Make function that returns length based on data type
        ptr += 4;
    }
    
    // Parse new template
    struct fds_template *new_tmplt = NULL;
    uint16_t parsed_len = new_len;
    if (fds_template_parse(FDS_TYPE_TEMPLATE, buffer, &parsed_len, &new_tmplt) != FDS_OK) {
        IPX_CTX_ERROR(pctx->ipx_ctx, "Failed to parse new template %u", new_id);
        free(buffer);
        return NULL;
    }
    
    // Link fields to IE Manager definitions so plugins know how to print them
    if (fds_template_ies_define(new_tmplt, ipx_ctx_iemgr_get(pctx->ipx_ctx), false) != FDS_OK) {
        IPX_CTX_ERROR(pctx->ipx_ctx, "Failed to define IEs for template %u", new_id);
        fds_template_destroy(new_tmplt);
        free(buffer);
        return NULL;
    }
    
    // Note: fds_template might reference the buffer (zero-copy), so we must NOT free it yet.
    // We store it in the cache node to be freed on plugin destroy.
    
    // Cache it
    struct template_node *node = malloc(sizeof(struct template_node));
    if (!node) {
        fds_template_destroy(new_tmplt);
        free(buffer);
        return NULL;
    }
    node->old_id = old_tmplt->id;
    node->new_id = new_id;
    node->new_tmplt = new_tmplt;
    node->raw_buffer = buffer; // Take ownership
    node->next = pctx->templates;
    pctx->templates = node;
    
    // Write Template Set to output
    // Set Header (4B) + Template Record (new_len)
    // Padding to 4B boundary
    uint16_t set_len = 4 + new_len;
    uint16_t padding = 0;
    if (set_len % 4 != 0) {
        padding = 4 - (set_len % 4);
        set_len += padding;
    }
    
    struct fds_ipfix_set_hdr set_hdr;
    set_hdr.flowset_id = htons(FDS_IPFIX_SET_TMPLT);
    set_hdr.length = htons(set_len);
    
    msg_builder_write(builder, &set_hdr, 4);
    msg_builder_write(builder, new_tmplt->raw.data, new_tmplt->raw.length);
    if (padding > 0) {
        uint8_t pad[4] = {0};
        msg_builder_write(builder, pad, padding);
    }
    return new_tmplt;
}

// If there is no match we still need to add a default value, e.g., zero for integers, empty string for strings.
static void add_value(tmp_match_t* match, msg_builder_s* builder, struct ipx_ipfix_record* ref) 
{
    switch (match->data_type)
    {
                    case FDS_ET_STRING:
                    case FDS_ET_OCTET_ARRAY:
                    {
                        const char *val = match->matched ? match->value : "";
                        size_t len = strlen(val);
                
                        if (len < 255) {
                            uint8_t l = (uint8_t)len;
                            msg_builder_write(builder, &l, 1);
                            msg_builder_write(builder, val, len);
                            ref->rec.size += (1 + len);
                        } else {
                            uint8_t header[3];
                            header[0] = 255;
                            *(uint16_t*)(header+1) = htons((uint16_t)len);
                            msg_builder_write(builder, header, 3);
                            msg_builder_write(builder, val, len);
                            ref->rec.size += (3 + len);
                        }
                        break;
                    }
                    case FDS_ET_UNSIGNED_8:
                    {
                        uint8_t v = match->matched ? (uint8_t)strtoul(match->value, NULL, 10) : 0;
                        msg_builder_write(builder, &v, 1);
                        ref->rec.size += 1;
                        break;
                    }
                    case FDS_ET_UNSIGNED_16:
                    {
                        uint16_t v = match->matched ? (uint16_t)strtoul(match->value, NULL, 10) : 0;
                        v = htons(v);
                        msg_builder_write(builder, &v, 2);
                        ref->rec.size += 2;
                        break;
                    }
                    case FDS_ET_UNSIGNED_32:
                    {
                        uint32_t v = match->matched ? (uint32_t)strtoul(match->value, NULL, 10) : 0;
                        v = htonl(v);
                        msg_builder_write(builder, &v, 4);
                        ref->rec.size += 4;
                        break;
                    }
                    case FDS_ET_UNSIGNED_64:
                    {
                        uint64_t v = match->matched ? (uint64_t)strtoull(match->value, NULL, 10) : 0;
                        v = htobe64(v);
                        msg_builder_write(builder, &v, 8);
                        ref->rec.size += 8;
                        break;
                    }
                    default:
                        printf("Unsupported data type for extension: %d\n", match->data_type);
                        break;
                    }   
}



int
ipx_plugin_process(ipx_ctx_t *ipx_ctx, void *data, ipx_msg_t *base_msg)
{
    struct plugin_ctx *pctx = (struct plugin_ctx *) data;
    ipx_msg_ipfix_t *msg = (ipx_msg_ipfix_t *) base_msg;
    
    // Calculate maximum extension details for buffer allocation
    size_t max_ext_len = pctx->config->max_extension_len;
    
    // Calculate maximum buffer size
    struct fds_ipfix_msg_hdr *orig_hdr = (struct fds_ipfix_msg_hdr *) ipx_msg_ipfix_get_packet(msg);
    uint32_t drec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);
    size_t orig_size = ntohs(orig_hdr->length);
    // Add extra space for potential new template sets + extended records
    size_t max_buffer_size = orig_size + (max_ext_len * drec_cnt) + 4096;
    
    uint8_t *buffer = calloc(1, max_buffer_size);
    if (!buffer) {
        IPX_CTX_ERROR(ipx_ctx, "Failed to allocate buffer");
        return IPX_ERR_NOMEM;
    }
    
    msg_builder_s builder;
    builder.msg = ipx_msg_ipfix_create(ipx_ctx, ipx_msg_ipfix_get_ctx(msg), buffer, 0);
    if (!builder.msg) {
        free(buffer);
        return IPX_ERR_NOMEM;
    }
    
    builder.buffer = buffer;
    builder.msg_len = 0;
    
    msg_builder_write(&builder, orig_hdr, sizeof(struct fds_ipfix_msg_hdr));
    
    struct ipx_ipfix_set *sets;
    size_t set_cnt;
    ipx_msg_ipfix_get_sets(msg, &sets, &set_cnt);
    
    int rc = IPX_OK;
    size_t drec_idx = 0;
    
    for (size_t s = 0; s < set_cnt; s++) {
        struct ipx_ipfix_set set = sets[s];
        uint16_t flowset_id = ntohs(set.ptr->flowset_id);
        
        if (flowset_id < FDS_IPFIX_SET_MIN_DSET) {
            rc = msg_builder_copy_set(&builder, &set);
            if (rc != IPX_OK) goto cleanup;
            continue;
        }
        
        // Data Set logic with splitting
        uint16_t current_set_id = 0;
        
        for (;;) {
            struct ipx_ipfix_record *record = ipx_msg_ipfix_get_drec(msg, drec_idx);
            if (!record || !record_belongs_to_set(set.ptr, &record->rec)) break;
            
            struct fds_drec *drec = &record->rec;
            tmp_match_t match[CONFIG_IDS_MAX];
            memset(match, 0, sizeof(match));
            int extension_count = 0;
            
            uint16_t target_id = flowset_id;
            struct fds_template *new_tmplt = get_or_create_extended_template(pctx, 
                                                                             drec->tmplt, 
                                                                             &builder, 
                                                                             pctx->config->ids,
                                                                             pctx->config->ids_count);
            if (!new_tmplt) {
                rc = IPX_ERR_NOMEM;
                goto cleanup;
            }
            target_id = new_tmplt->id;                      

            // Manage Set Boundaries
            if (target_id != current_set_id) {
                if (current_set_id != 0) {
                    rc = msg_builder_end_dset(&builder);
                    if (rc != IPX_OK) goto cleanup;
                }
                msg_builder_begin_dset(&builder, target_id);
                current_set_id = target_id;
            }
            
            struct ipx_ipfix_record *ref = ipx_msg_ipfix_add_drec_ref(&builder.msg);
            if (!ref) {
                rc = IPX_ERR_NOMEM;
                goto cleanup;
            }
            
            memcpy(&ref->rec, drec, sizeof(struct fds_drec));
            ref->rec.data = builder.buffer + builder.msg_len;
            
            msg_builder_write(&builder, drec->data, drec->size);
            

            for (int j = 0; j < pctx->config->ids_count; j++) {
                match[extension_count].id = pctx->config->ids[j].id;
                match[extension_count].data_type = pctx->config->ids[j].data_type;
                match[extension_count].matched = false;
                for( int v =0; v < pctx->config->ids[j].values_count; v++) {
                    config_value_t* value = &pctx->config->ids[j].values[v];
                    if (fds_ipfix_filter_eval_biflow(value->filter, drec) != FDS_IPFIX_FILTER_NO_MATCH) {
                        match[extension_count].matched = true;
                        match[extension_count].value = value->value;
                        break;
                    }
                }
                extension_count++;
            }
            
            // Update template pointer in the new record
            ref->rec.tmplt = new_tmplt;
            drec->tmplt = new_tmplt;                
            // Encode Variable Length String
            for( int k = 0; k < extension_count; k++) {                
                add_value(&match[k], &builder, ref);
            }
            
            drec_idx++;
        }
        
        if (current_set_id != 0) {
            rc = msg_builder_end_dset(&builder);
            if (rc != IPX_OK) goto cleanup;
        }
    }
    
    msg_builder_finish(&builder);
    ipx_msg_ipfix_destroy(msg);
    if (msg_builder_is_empty_msg(&builder)) {
        ipx_msg_ipfix_destroy(builder.msg);
    } else {
        ipx_ctx_msg_pass(ipx_ctx, (ipx_msg_t *) builder.msg);
    }
    return IPX_OK;
    
cleanup:
    ipx_msg_ipfix_destroy(builder.msg);
    IPX_CTX_ERROR(ipx_ctx, "Failed to build extended message");
    return rc;
}