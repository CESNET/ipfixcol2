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
#include <ipfixcol2.h>
#include <ctype.h>
#include <inttypes.h>

/** Default size of the mapping database */
#define DEF_SIZE 32
/** Size of error buffer                 */
#define ERR_SIZE 256

/** Internal structure of the mapping database */
struct map_s {
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
map_init()
{
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
    return res;
}

void
map_clear(map_t *map)
{
    for (size_t i = 0; i < map->rec_size; ++i) {
        struct map_rec *rec = map->rec_array[i];
        free(rec->unirec.name);
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
    if (!new_rec->unirec.name) {
        snprintf(map->err_buffer, ERR_SIZE, "Memory allocation error");
        free(new_rec);
        return IPX_ERR_NOMEM;
    }

    map->rec_array[map->rec_size++] = new_rec;
    return IPX_OK;
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
map_load_line(map_t *map, char *line, size_t line_id)
{
    const char *delim = " \t";
    char *save_ptr = NULL;

    char *ur_name;
    ur_field_type_t ur_type;

    // Get the name
    char *token = strtok_r(line, delim, &save_ptr);
    if (!token) {
        // Skip empty line
        return IPX_OK;
    }

    ur_name = strdup(token);
    if (!ur_name) {
        snprintf(map->err_buffer, ERR_SIZE, "Memory allocation error");
        return IPX_ERR_NOMEM;
    }

    // Get the type
    token = strtok_r(NULL, delim, &save_ptr);
    if (!token) {
        snprintf(map->err_buffer, ERR_SIZE, "Line %zu: Unexpected end of line!", line_id);
        free(ur_name);
        return IPX_ERR_FORMAT;
    }

    int type = ur_get_field_type_from_str(token);
    if (type == UR_E_INVALID_TYPE) {
        snprintf(map->err_buffer, ERR_SIZE, "Line %zu: Invalid type '%s' of UniRec field '%s'",
            line_id, token, ur_name);
        free(ur_name);
        return IPX_ERR_FORMAT;
    }
    ur_type = type;

    struct map_rec rec;
    rec.unirec.name = ur_name;
    rec.unirec.type = ur_type;

    // Get list of IPFIX fields
    token = strtok_r(NULL, delim, &save_ptr);
    if (!token) {
        snprintf(map->err_buffer, ERR_SIZE, "Line %zu: Unexpected end of line!", line_id);
        free(ur_name);
        return IPX_ERR_FORMAT;
    }

    // Process IPFIX fields
    char *subsave_ptr = NULL;
    int rc = IPX_OK;
    for (char *ies = token; rc == IPX_OK; ies = NULL) {
        // Get the next token
        char *subtoken = strtok_r(ies, ",", &subsave_ptr);
        if (!subtoken) {
            break;
        }

        // Parse IPFIX specifier
        char aux;
        if (sscanf(subtoken, "e%"SCNu32"id%"SCNu16"%c", &rec.ipfix.en, &rec.ipfix.id, &aux) != 2) {
            snprintf(map->err_buffer, ERR_SIZE, "Line %zu: Invalid IPFIX specifier '%s'",
                line_id, subtoken);
            rc = IPX_ERR_FORMAT;
            break;
        }

        // Store the record
        rc = map_rec_add(map, &rec);
    }

    free(ur_name);
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
        return IPX_ERR_DENIED;
    }

    char *line_ptr = NULL;
    size_t line_size = 0;
    size_t line_cnt = 0;

    // Process each line
    int rc = IPX_OK;
    while (rc == IPX_OK && getline(&line_ptr, &line_size, ur_file) != -1) {
        line_cnt++;

        // Trim leading whitespaces
        char *line_trim = line_ptr;
        while (isspace((int) (*line_trim))) {
            line_trim++;
        }

        if (line_trim[0] == '#') {
            // Skip a comment
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
        if (rec_prev->ipfix.en != rec_now->ipfix.en || rec_prev->ipfix.id != rec_now->ipfix.id) {
            rec_prev = rec_now;
            continue;
        }

        // Collision detected!
        snprintf(map->err_buffer, ERR_SIZE, "The same IPFIX IE (PEN %" PRIu32 ", ID %" PRIu16 ") "
            "is mapped to different UniRec fields ('%s' and '%s')",
            rec_now->ipfix.en, rec_now->ipfix.id, rec_now->unirec.name, rec_prev->unirec.name);
        map_clear(map);
        return IPX_ERR_FORMAT;
    }

    return IPX_OK;
}
