/**
 * \file map.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief IPFIX-to-Nemea mapping database (header file)
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

#ifndef UNIREC_MAP_H
#define UNIREC_MAP_H

#include <stdint.h>
#include <unirec/unirec.h>
#include <ipfixcol2.h>

/** Internal map type */
typedef struct map_s map_t;

/** IPFIX-to-UniRec mapping record          */
struct map_rec {
    struct {
        /** Private Enterprise Number       */
        uint32_t en;
        /** Information Element ID          */
        uint16_t id;
    } ipfix; /**< IPFIX specific parameters */

    struct {
        /** Field name                      */
        char *name;
        /** Data type                       */
        ur_field_type_t type;
    } unirec;
};

/**
 * \brief Initialize a mapping database
 * \return Pointer to the new DB
 */
map_t *
map_init();

/**
 * \brief Destroy a mapping database
 * \param[in] map Mapping database
 */
void
map_destroy(map_t *map);

/**
 * \brief Load a mapping database from a file
 *
 * \note In case of an error, an error message is filled (see map_last_error())
 * \param[in] map  Mapping database
 * \param[in] file Path to the file
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED if it is unable to open the file
 * \return #IPX_ERR_FORMAT if the file is malformed
 * \return #IPX_ERR_NOMEM  if a memory allocation error has occurred
 */
int
map_load(map_t *map, const char *file);

/**
 * \brief Get number of mapping records
 * \param[in] map Mapping database
 * \return Number of records
 */
size_t
map_size(const map_t *map);

/**
 * \brief Get a mapping record
 *
 * \param[in] map Mapping database
 * \param[in] idx Index of the record (starts from 0)
 * \return Pointer to the record or NULL (index is out of range)
 */
const struct map_rec *
map_get(const map_t *map, size_t idx);

/**
 * \brief Remove all records
 * \param[in] map Mapping database
 */
void
map_clear(map_t *map);

/**
 * \brief Get the last error message
 * \param[in] map Mapping database
 * \return Pointer to the last error message
 */
const char *
map_last_error(map_t *map);

#endif //UNIREC_MAP_H
