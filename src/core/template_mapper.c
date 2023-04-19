/**
 * \file template_mapper.c
 * \author Adam Zvara <xzvara01@stud.fit.vutbr.cz>
 * \brief Mapping from orignal to modified templates
 * \date 2023
 */

#include "template_mapper.h"

/** Index of ID used to address mapper
 *  Since template IDs from 0 to 255 are reserved, we need to shift
 *  the ID by 256 to get the correct index
 */
#define IPX_MAPPER_GET_TMPLT_ID(id) ((id) - FDS_IPFIX_SET_MIN_DSET)

/** Intervals in which templates IDs are separated into groups
 *  Must be power of 2!
 */
#define IPX_MAPPER_L2_RANGE 256u

/** Default amount of items in template mapper
 *  (65536 - 256) / IPX_MAPPER_L2_RANGE
 */
#define IPX_MAPPER_L1_RANGE 255u

/** Macros for getting indexes of L1 and L2 tables                  */
#define IPX_MAPPER_L1_INDEX(id) (IPX_MAPPER_GET_TMPLT_ID((id)) >> 8)
#define IPX_MAPPER_L2_INDEX(id) {                               \
    (IPX_MAPPER_GET_TMPLT_ID((id)) & (IPX_MAPPER_L2_RANGE - 1)  \
)}

/** \brief Template mapper field                                    */
struct mapper_field {
    /** Data used by mapper callbacks                               */
    struct modified_tmplt_id ident;
    /** Pointer to modified template                                */
    const struct fds_template *modified_tmplt;
    /** Next item in linked list                                    */
    struct mapper_field *next;
};

/** \brief Range of template IDs used for mapping                   */
struct mapper_l2_table {
    /** Linked list of template mappings                            */
    struct mapper_field *fields;
};

/** \brief Range of template IDs used for mapping                   */
struct mapper_l1_table {
    /** Linked list of template mappings                            */
    struct mapper_l2_table l2_table[IPX_MAPPER_L2_RANGE];
};

/** \brief Template mapper                                          */
struct template_mapper {
    /** Number of templates in mapper                               */
    size_t template_cnt;
    /** Mapping fields                                              */
    struct mapper_l1_table *l1_table[IPX_MAPPER_L1_RANGE];
};

/**
 * \brief Destroy mapper field
 *
 * \param[in] field Pointer to mapper field
 */
static inline void
mapper_field_destroy(struct mapper_field *field)
{
    free(field->ident.data);
    free(field);
}

struct template_mapper *
ipx_mapper_create()
{
    struct template_mapper *map = calloc(1, sizeof(*map));
    if (map == NULL) {
        return NULL;
    }

    return map;
}

size_t
ipx_mapper_get_tmplt_count(ipx_template_mapper_t *map)
{
    return map->template_cnt;
}

typedef void (*mapper_l1_cb)(void *ptr);

static void
mapper_item_cleanup(struct template_mapper *map, mapper_l1_cb l1_cb)
{
    // Go through all L1 tables
    for (size_t i = 0; i < IPX_MAPPER_L1_RANGE; i++) {
        if (map->l1_table[i] == NULL) {
            continue;
        }

        // Go through all L2 tables
        for (size_t j = 0; j < IPX_MAPPER_L2_RANGE; j++) {
            struct mapper_field *orig_field = map->l1_table[i]->l2_table[j].fields;
            struct mapper_field *field = orig_field;

            // Go through all fields in current L2 table
            while (field != NULL) {
                struct mapper_field *next = field->next;
                mapper_field_destroy(field);
                field = next;
            }

            orig_field = NULL;
        }

        // Apply callback to L1 table
        if (l1_cb != NULL) {
            l1_cb(map->l1_table[i]);
        }

    }
}

void
ipx_mapper_clear(struct template_mapper *map)
{
    mapper_item_cleanup(map, NULL);
    map->template_cnt = 0;
}

void
ipx_mapper_destroy(struct template_mapper *map)
{
    mapper_item_cleanup(map, free);
    free(map);
}

/**
 * \brief Create new L1 table
 *
 * \return Pointer to new L1 table or NULL on memory allocation error
 */
struct mapper_l1_table *
mapper_create_l1_table()
{
    struct mapper_l1_table *table = calloc(1, sizeof(*table));
    if (table == NULL) {
        return NULL;
    }

    return table;
}

/**
 * \brief Get index of L1 table
 *
 * \param[in] id ID of template
 * \return Index of L1 table
 */
static inline size_t
mapper_get_l1_idx(uint16_t id)
{
    size_t l1_idx = IPX_MAPPER_L1_INDEX(id);
    assert(l1_idx < IPX_MAPPER_L1_RANGE);
    return l1_idx;
}

/**
 * \brief Get index of L2 table
 *
 * \param[in] id ID of template
 * \return Index of L2 table
 */
static inline size_t
mapper_get_l2_idx(uint16_t id)
{
    size_t l2_idx = IPX_MAPPER_L2_INDEX(id);
    assert(l2_idx < IPX_MAPPER_L2_RANGE);
    return l2_idx;
}

/**
 * \brief Store new field into mapper
 *
 * \param[in] map          Template mapper
 * \param[in] original_id  Original template ID
 * \param[in] new_field    New field to store
 * \return #IPX_OK on success, #IPX_ERR_NOMEM on memory allocation error
 */
static int
mapper_store_field(struct template_mapper *map, uint16_t original_id, struct mapper_field *new_field)
{
    // Get index of L1 table
    size_t l1_idx = mapper_get_l1_idx(original_id);
    struct mapper_l1_table *l1_table = map->l1_table[l1_idx];

    // If L1 table doesn't exist, create it
    if (l1_table == NULL) {
        if ((l1_table = mapper_create_l1_table()) == NULL) {
            return IPX_ERR_NOMEM;
        }
        struct mapper_l1_table **tab = &(map->l1_table[l1_idx]);
        *tab = l1_table;
    }

    // Get index of L2 table
    size_t l2_idx = mapper_get_l2_idx(original_id);
    struct mapper_l2_table *l2_table = &(l1_table->l2_table[l2_idx]);
    struct mapper_field **fields = &(l2_table->fields);

    // Finally store the field in linked list
    new_field->next = *fields;
    *fields = new_field;

    return IPX_OK;
}

int
ipx_mapper_add(ipx_template_mapper_t *map, const struct fds_template *modified_tmplt, struct modified_tmplt_id *item, uint16_t original_id)
{
    // Create new field
    struct mapper_field *new_field = malloc(sizeof(*new_field));
    if (new_field == NULL) {
        return IPX_ERR_NOMEM;
    }

    new_field->ident.data = malloc(item->length);
    if (new_field->ident.data == NULL) {
        free(new_field);
        return IPX_ERR_NOMEM;
    }


    new_field->ident.length = item->length;
    memcpy(new_field->ident.data, item->data, item->length);
    memcpy(new_field->ident.appended_fields, item->appended_fields, APPENDED_FIELDS_DEF_CNT * sizeof(uint8_t));

    // Store modifed template in new field
    new_field->modified_tmplt = modified_tmplt;

    // Store field in mapper
    if (mapper_store_field(map, original_id, new_field) != IPX_OK) {
        mapper_field_destroy(new_field);
        return IPX_ERR_NOMEM;
    }
    map->template_cnt++;

    return IPX_OK;
}


static inline int
item_cmp(const struct modified_tmplt_id *item1, const struct modified_tmplt_id *item2)
{
    // Compare original templates
    if (item1->length != item2->length) {
        return -1;
    }

    // Compare raw templates except template ID
    if (memcmp(item1->data+2, item2->data+2, item1->length-2)) {
        return -1;
    }

    // Compare fields
    if (memcmp(item1->appended_fields, item2->appended_fields, APPENDED_FIELDS_DEF_CNT)) {
        return -1;
    }

    return IPX_OK;
}

const struct fds_template *
ipx_mapper_lookup(ipx_template_mapper_t *map, const struct modified_tmplt_id *item, uint16_t original_id)
{
    // Get index of L1 table
    size_t l1_idx = mapper_get_l1_idx(original_id);
    struct mapper_l1_table *l1_table = map->l1_table[l1_idx];

    if (l1_table == NULL) {
        return NULL;
    }

    // Get index of L2 table
    size_t l2_idx = mapper_get_l2_idx(original_id);
    struct mapper_l2_table *l2_table = &(l1_table->l2_table[l2_idx]);

    struct mapper_field *field = l2_table->fields;
    while (field != NULL) {
        if (item_cmp(&field->ident, item) == IPX_OK) {
            return field->modified_tmplt;
        }

        field = field->next;
    }
    return NULL;
}