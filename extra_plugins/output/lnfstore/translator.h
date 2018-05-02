/**
 * \file translator.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Conversion of IPFIX to LNF format (header file)
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

#ifndef LS_TRANSLATOR_H
#define LS_TRANSLATOR_H

#include <ipfixcol2.h>

/** Internal definition of translator */
typedef struct translator_s translator_t;

/**
 * \brief Create a new instance of a translator
 * \return Pointer to the instance or NULL.
 */
translator_t *
translator_init(ipx_ctx_t *ctx);

/**
 * \brief Destroy instance of a translator
 * \param[in] trans Instance
 */
void
translator_destroy(translator_t *trans);

/**
 * \brief Convert a IPFIX record to a LNF record
 * \warning LNF record is always automatically cleared before conversion start.
 * \param[in]     trans     Translator instance
 * \param[in]     ipfix_rec IPFIX record (read only!)
 * \param[in,out] lnf_rec   Filled LNF record
 * \param[in]     flags     Flags for iterator over the IPFIX record
 * \return Number of converted fields
 */
int
translator_translate(translator_t *trans, struct fds_drec *ipfix_rec, lnf_rec_t *lnf_rec,
    uint16_t flags);

#endif //LS_TRANSLATOR_H
