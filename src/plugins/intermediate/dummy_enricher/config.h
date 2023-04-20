/**
 * \file src/plugins/intermediate/asn/asn.c
 * \author Adam Zvara <xzvara01@stud.fit.vutbr.cz>
 * \brief Configuration parser of ASN plugin (header file)
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

#ifndef ASN_CONFIG_H
#define ASN_CONFIG_H

#include <ipfixcol2.h>
#include "stdint.h"

enum FIELD_TYPE {
    DUMMY_FIELD_STRING,
    DUMMY_FIELD_INT,
    DUMMY_FIELD_DOUBLE
};

struct new_field {
    uint32_t pen;
    uint16_t id;
    enum FIELD_TYPE type;
    union {
        char *string;
        int integer;
        double decimal;
    } value;
    uint16_t times;
};

/** Configuration of a instance of the dummy plugin     */
struct dummy_config {
    /** Amount of parsed fields                         */
    uint16_t fields_count;
    /** Array of parsed fields                          */
    struct new_field *fields;
};

/**
 * \brief Parse configuration of the plugin
 * \param[in] ctx    Instance context
 * \param[in] params XML parameters
 * \return Pointer to the parse configuration of the instance on success
 * \return NULL if arguments are not valid or if a memory allocation error has occurred
 */
struct dummy_config *
config_parse(ipx_ctx_t *ctx, const char *params);

/**
 * \brief Destroy parsed configuration
 * \param[in] cfg Parsed configuration
 */
void
config_destroy(struct dummy_config *cfg);

#endif // ASN_CONFIG_H
