/**
 * \file translator.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \author Tomas Cejka <cejkat@cesnet.cz>
 * \brief Conversion of IPFIX to UniRec format (source file)
 */

/* Copyright (C) 2015-2018 CESNET, z.s.p.o.
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

#ifndef UR_TRANSLATOR_H
#define UR_TRANSLATOR_H

#include <ipfixcol2.h>
#include "map.h"

/** Internal translator structure        */
typedef struct translator_s translator_t;

/**
 * \brief Create a new instance of a translator
 *
 * \note
 *   UniRec template \p tmplt MUST exists and cannot be modified if the
 *   translator exists.
 * \param[in] ctx        Plugin instance context (only for log!)
 * \param[in] map        Mapping database with already loaded mapping file
 * \param[in] tmplt      Parsed UniRec template that will be used to format records
 * \param[in] tmplt_spec UniRec template specification with question marks (describes
 *   optional UniRec fields)
 * \return Pointer to the instance or NULL.
 */
translator_t *
translator_init(ipx_ctx_t *ctx, const map_t *map, const ur_template_t *tmplt,
    const char *tmplt_spec);

/**
 * \brief Destroy instance of a translator
 * \param[in] trans Instance
 */
void
translator_destroy(translator_t *trans);

/**
 * \brief Set IPFIX message context
 *
 * This function MUST be called before processing each IPFIX Message to set proper record
 * parameters. The message header is required for determine ODID or other parameters.
 * \param[in] trans Translator instance
 * \param[in] hdr   IPFIX message header
 */
void
translator_set_context(translator_t *trans, const struct fds_ipfix_msg_hdr *hdr);

/**
 * \brief Convert a IPFIX record to an UniRec message
 * \param[in]  trans     Translator instance
 * \param[in]  ipfix_rec IPFIX record (read only!)
 * \param[in]  flags     Flags for iterator over the IPFIX record
 * \param[out] size      Size of the converted message
 * \return Number of converted fields
 */
const void *
translator_translate(translator_t *trans, struct fds_drec *ipfix_rec, uint16_t flags,
    uint16_t *size);

#endif // UR_TRANSLATOR_H
