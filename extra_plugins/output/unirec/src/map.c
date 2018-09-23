/**
 * \file map.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief IPFIX-to-Nemea mapping database (source file)
 * \date September 2018
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
 * This software is provided ``as is, and any express or implied
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

#include "map.h"
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <ipfixcol2.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>

/** Default size of the mapping database */
#define DEF_SIZE 32
/** Size of error buffer                 */
#define ERR_SIZE 256

/** Internal structure of the mapping database */
struct map_s {
    /** Manager of IPFIX Information Elements  */
    const fds_iemgr_t *iemgr;

    /** Size of valid records                  */
    size_t rec_size;
    /** Size of pre-allocated array            */
    size_t rec_alloc;
    /** Array of pointers to loaded records    */
    struct map_rec **rec_array;

    /** Error buffer                           */
    char err_buffer[ERR_SIZE];
};

map_t *
map_init(const fds_iemgr_t *ie_mgr)
{
    if (!ie_mgr) {
        return NULL;
    }

    struct map_s *res = calloc(1, sizeof(*res));
    if (!res) {
        return NULL;
    }

    res->rec_size = 0;
    res->rec_alloc = DEF_SIZE;
    res->rec_array = malloc(res->rec_alloc * sizeof(*res->rec_array));
    if (!res->rec_array) {
        free(res);
        return NULL;
    }

    snprintf(res->err_buffer, ERR_SIZE, "No error");
    res->iemgr = ie_mgr;
    return res;
}

void
map_clear(map_t *map)
{
    for (size_t i = 0; i < map->rec_size; ++i) {
        struct map_rec *rec = map->rec_array[i];
        free(rec->unirec.name);
        free(rec->unirec.type_str);
        free(rec);
    }

    map->rec_size = 0;
}

void
map_destroy(map_t *map)
{
    map_clear(map);
    free(map->rec_array);
    free(map);
}

size_t
map_size(const map_t *map)
{
    return map->rec_size;
}

const struct map_rec *
map_get(const map_t *map, size_t idx)
{
    if (idx >= map->rec_size) {
        return NULL;
    }

    return map->rec_array[idx];
}

const char *
map_last_error(map_t *map)
{
    return map->err_buffer;
}

/**
 * \brief Add a record to the mapping database
 * \param[in] map Mapping database
 * \param[in] rec Record to add
 * \return #IPX_OK on success
 * \return #IPX_ERR_NOMEM on failure (an error message is set)
 */
static int
map_rec_add(map_t *map, const struct map_rec *rec)
{
    if (map->rec_alloc == map->rec_size) {
        const size_t new_alloc = 2 * map->rec_alloc;
        const size_t alloc_size = new_alloc * sizeof(*map->rec_array);
        struct map_rec **new_array = realloc(map->rec_array, alloc_size);
        if (!new_array) {
            snprintf(map->err_buffer, ERR_SIZE, "Memory allocation error");
            return IPX_ERR_NOMEM;
        }

        map->rec_array = new_array;
        map->rec_alloc = new_alloc;
    }

    // Make a copy of the record
    struct map_rec *new_rec = malloc(sizeof(*new_rec));
    if (!new_rec) {
        snprintf(map->err_buffer, ERR_SIZE, "Memory allocation error");
        return IPX_ERR_NOMEM;
    }

    *new_rec = *rec;
    new_rec->unirec.name = strdup(rec->unirec.name);
    new_rec->unirec.type_str = strdup(rec->unirec.type_str);
    if (!new_rec->unirec.name || !new_rec->unirec.type_str) {
        snprintf(map->err_buffer, ERR_SIZE, "Memory allocation error");
        free(new_rec->unirec.name);
        free(new_rec->unirec.type_str);
        free(new_rec);
        return IPX_ERR_NOMEM;
    }

    map->rec_array[map->rec_size++] = new_rec;
    return IPX_OK;
}

/**
 * \brief Get the definition of an IPFIX Information Element
 * \param[in] mgr  Manager of IPFIX Information Elements (IE)
 * \param[in] elem Information Element to find
 * \return Pointer to the definition or NULL (unknown or invalid IE specifier)
 */
static const struct fds_iemgr_elem *
map_elem_get_ipfix(const fds_iemgr_t *mgr, const char *elem)
{
    const struct fds_iemgr_elem *res;

    // Try to find the identifier
    res = fds_iemgr_elem_find_name(mgr, elem);
    if (res != NULL) {
        return res;
    }

    // Try to parse "old style" specifier
    uint32_t ipfix_en;
    uint16_t ipfix_id;

    char aux;
    if (sscanf(elem, "e%"SCNu32"id%"SCNu16"%c", &ipfix_en, &ipfix_id, &aux) != 2) {
        return NULL;
    }

    return fds_iemgr_elem_find_id(mgr, ipfix_en, ipfix_id);
}

/**
 * \brief Get the type of internal function
 * \param[in] elem Identification of an internal function to find
 * \return Function identifier or ::MAP_SRC_INVALID in case or failure
 */
static enum MAP_SRC
map_elem_get_internal(const char *elem)
{
    if (strcmp(elem, "_internal_lbf_") == 0) {
        return MAP_SRC_INTERNAL_LBF;
    } else if (strcmp(elem, "_internal_dbf_") == 0) {
        return MAP_SRC_INTERNAL_DBF;
    } else {
        return MAP_SRC_INVALID;
    }
}

/**
 * \brief Parse list of IPFIX IE and add mapping records to the database
 * \param[in] map         Mapping database
 * \param[in] ur_name     UniRec name
 * \param[in] ur_type     UniRec type
 * \param[in] ur_type_str UniRec type string
 * \param[in] ie_defs     Definition of IPFIX IEs (comma separated list of definitions)
 * \param[in] line_id Line ID (just for error messages)
 * \return #IPX_OK on success
 * \return #IPX_ERR_NOMEM or #IPX_ERR_FORMAT on failure
 */
static int
map_load_line_ie_defs(map_t *map, char *ur_name, int ur_type, char *ur_type_str,
    const char *ie_defs, size_t line_id)
{
    char *defs_cpy = strdup(ie_defs);
    if (!defs_cpy) {
        snprintf(map->err_buffer, ERR_SIZE, "Memory allocation error");
        return IPX_ERR_NOMEM;
    }

    // Remove whitespaces from list of IPFIX IE definitions
    size_t w_idx;
    for (w_idx = 0; (*ie_defs) != '\0'; ++ie_defs) {
        if (isspace((int) *ie_defs)) {
            continue;
        }
        defs_cpy[w_idx++] = *ie_defs;
    }
    defs_cpy[w_idx] = '\0';

    // Prepare the mapping record
    struct map_rec rec;
    rec.unirec.name = ur_name;
    rec.unirec.type = ur_type;
    rec.unirec.type_str = ur_type_str;

    // Process IPFIX fields
    char *subsave_ptr = NULL;
    int rc = IPX_OK;
    for (char *ies = defs_cpy; rc == IPX_OK; ies = NULL) {
        // Get the next token
        char *subtoken = strtok_r(ies, ",", &subsave_ptr);
        if (!subtoken) {
            break;
        }

        // Parse IPFIX specifier
        const struct fds_iemgr_elem *elem_def = map_elem_get_ipfix(map->iemgr, subtoken);
        if (elem_def != NULL) {
            // Store the "IPFIX element" record
            rec.ipfix.source = MAP_SRC_IPFIX;
            rec.ipfix.def = elem_def;
            rec.ipfix.id = elem_def->id;
            rec.ipfix.en = elem_def->scope->pen;
            rc = map_rec_add(map, &rec);
            continue;
        }

        enum MAP_SRC fn_id = map_elem_get_internal(subtoken);
        if (fn_id != MAP_SRC_INVALID) {
            // Store the "Internal function" record
            rec.ipfix.source = fn_id;
            rec.ipfix.def = NULL;
            rec.ipfix.id = 0;
            rec.ipfix.en = 0;
            rc = map_rec_add(map, &rec);
            continue;
        }

        snprintf(map->err_buffer, ERR_SIZE, "Line %zu: IPFIX specifier '%s' is invalid or "
            "a definition of the Information Element is missing! For more information, see "
            "the plugin documentation.", line_id, subtoken);
        rc = IPX_ERR_FORMAT;
        break;
    }

    free(defs_cpy);
    return rc;
}

/**
 * \brief Parse a line of a configuration file and add records to the database
 * \param[in] map     Mapping database
 * \param[in] line    Line to parse
 * \param[in] line_id Line ID (just for error messages)
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT or #IPX_ERR_NOMEM on failure (an error message is set)
 */
static int
map_load_line(map_t *map, const char *line, size_t line_id)
{
    int rc = IPX_OK;
    char *ur_name = NULL;
    char *ur_type_str = NULL;
    ur_field_type_t ur_type;

    char *line_cpy = strdup(line);
    if (!line_cpy) {
        snprintf(map->err_buffer, ERR_SIZE, "Memory allocation error");
        return IPX_ERR_NOMEM;
    }

    const char *delim = " \t";
    char *save_ptr = NULL;

    // Get the name
    char *token = strtok_r(line_cpy, delim, &save_ptr);
    if (!token) {
        // Skip empty line
        goto end;
    }

    ur_name = strdup(token);
    if (!ur_name) {
        snprintf(map->err_buffer, ERR_SIZE, "Memory allocation error");
        rc = IPX_ERR_NOMEM;
        goto end;
    }

    // Get the type
    token = strtok_r(NULL, delim, &save_ptr);
    if (!token) {
        snprintf(map->err_buffer, ERR_SIZE, "Line %zu: Unexpected end of line!", line_id);
        rc = IPX_ERR_FORMAT;
        goto end;
    }

    int type = ur_get_field_type_from_str(token);
    if (type == UR_E_INVALID_TYPE) {
        snprintf(map->err_buffer, ERR_SIZE, "Line %zu: Invalid type '%s' of UniRec field '%s'",
            line_id, token, ur_name);
        rc = IPX_ERR_FORMAT;
        goto end;
    }
    ur_type = type;

    ur_type_str = strdup(token);
    if (!ur_type_str) {
        snprintf(map->err_buffer, ERR_SIZE, "Memory allocation error");
        rc = IPX_ERR_NOMEM;
        goto end;
    }

    // Get the start position of the list of IPFIX fields
    token = strtok_r(NULL, delim, &save_ptr);
    if (!token) {
        snprintf(map->err_buffer, ERR_SIZE, "Line %zu: Unexpected end of line!", line_id);
        rc = IPX_ERR_FORMAT;
        goto end;
    }

    ptrdiff_t offset = token - line_cpy;
    const char *ie_defs = line + offset;
    rc = map_load_line_ie_defs(map, ur_name, ur_type, ur_type_str, ie_defs, line_id);

end:
    free(ur_type_str);
    free(ur_name);
    free(line_cpy);
    return rc;
}

/**
 * \brief Compare two mapping record (auxiliary function)
 * \param[in] p1 Pointer to a pointer to a mapping record
 * \param[in] p2 Pointer to a pointer to a mapping record
 * \return An integer less than, equal to, or greater than zero if the \p p1 is found, respectively,
 *  to be less than, to match, or be greater than the \p p2.
 */
static int
map_sort_fn(const void *p1, const void *p2)
{
    struct map_rec *rec1 = *(struct map_rec **) p1;
    struct map_rec *rec2 = *(struct map_rec **) p2;

    if (rec1->ipfix.source != rec2->ipfix.source) {
        return (rec1->ipfix.source < rec2->ipfix.source) ? (-1) : 1;
    }

    // Primary sort by PEN
    if (rec1->ipfix.en != rec2->ipfix.en) {
        return (rec1->ipfix.en < rec2->ipfix.en) ? (-1) : 1;
    }

    // Secondary sort by ID
    if (rec1->ipfix.id != rec2->ipfix.id) {
        return (rec1->ipfix.id < rec2->ipfix.id) ? (-1) : 1;
    }

    return 0;
}

int
map_load(map_t *map, const char *file)
{
    // Clear previous data
    map_clear(map);

    // Open the file
    FILE *ur_file = fopen(file, "r");
    if (!ur_file) {
        const char *err_str;
        ipx_strerror(errno, err_str);
        snprintf(map->err_buffer, ERR_SIZE, "Failed to open configuration file '%s': %s",
            file, err_str);
        return IPX_ERR_DENIED;
    }

    char *line_ptr = NULL;
    size_t line_size = 0;
    size_t line_cnt = 0;

    // Process each line
    int rc = IPX_OK;
    while (rc == IPX_OK && getline(&line_ptr, &line_size, ur_file) != -1) {
        line_cnt++;

        // Remove possible comments
        char *comm_pos = strchr(line_ptr, '#');
        if (comm_pos != NULL) {
            *comm_pos = '\0'; // Terminate the line here
        }

        // Trim leading whitespaces
        char *line_trim = line_ptr;
        while (isspace((int) (*line_trim))) {
            line_trim++;
        }

        if (strlen(line_trim) == 0) {
            // Skip empty line
            continue;
        }

        // Parse the line
        rc = map_load_line(map, line_trim, line_cnt);
    }

    // Cleanup
    free(line_ptr);
    fclose(ur_file);

    if (rc != IPX_OK) {
        map_clear(map);
        return rc;
    }

    if (map->rec_size == 0) {
        // Empty
        return IPX_OK;
    }

    // Sort and check for mapping collisions
    const size_t size = sizeof(*map->rec_array);
    qsort(map->rec_array, map->rec_size, size, map_sort_fn);

    const struct map_rec *rec_prev = map->rec_array[0];
    for (size_t i = 1; i < map->rec_size; i++) {
        // Records are sorted, therefore, the same IPFIX IEs should be next to each other
        const struct map_rec *rec_now = map->rec_array[i];
        if (rec_prev->ipfix.source != MAP_SRC_IPFIX || rec_now->ipfix.source != MAP_SRC_IPFIX) {
            rec_prev = rec_now;
            continue;
        }

        if (rec_prev->ipfix.en != rec_now->ipfix.en || rec_prev->ipfix.id != rec_now->ipfix.id) {
            rec_prev = rec_now;
            continue;
        }

        // Collision detected!
        snprintf(map->err_buffer, ERR_SIZE, "The IPFIX IE '%s' (PEN %" PRIu32 ", ID %" PRIu16 ") "
            "is mapped to multiple different UniRec fields ('%s' and '%s')",
            rec_now->ipfix.def->name, rec_now->ipfix.en, rec_now->ipfix.id,
            rec_now->unirec.name, rec_prev->unirec.name);
        map_clear(map);
        return IPX_ERR_FORMAT;
    }

    return IPX_OK;
}
