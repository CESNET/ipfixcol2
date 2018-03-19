/**
 * \file src/core/parser.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief IPFIX Message parser (header file)
 * \date 2018
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

#ifndef IPFIXCOL_PROCESSOR_H
#define IPFIXCOL_PROCESSOR_H

#include <stdint.h>
#include <ipfixcol2/api.h>
#include <ipfixcol2/session.h>
#include <ipfixcol2/message_garbage.h>
#include <ipfixcol2/message_ipfix.h>
#include "ipfixcol2/plugins.h"

// TODO: what IPFIX parser provedes... parse messages, provides templates management,

/** Internal data type of parser                                                                 */
typedef struct ipx_parser ipx_parser_t;

/**
 * \brief Create a IPFIX parser
 *
 * Context is used for proper identification of the plugin in case of any errors. Moreover, the
 * parser automatically generates garbage messages and sends them.
 * \param[in] ctx Plugin context
 * \param[in] fb  Feedback pipe (only for Input plugins to receive a request to close particular
 *   Transport Session)
 * \return Pointer the parser or NULL (memory allocation error)
 */
IPX_API ipx_parser_t *
ipx_parser_create(ipx_ctx_t *ctx, ipx_fpipe *fb);

/**
 * \brief Destroy an IPFIX parser
 *
 * \warning All template managers and their templates will be also immediately destroyed.
 * \param[in] parser Message parser
 */
IPX_API void
ipx_parser_destroy(ipx_parser_t *parser);

/**
 * \brief Process IPFIX (or NetFlow) Message
 *
 * If the message is in the form of NetFlow, it is converted to IPFIX first. Parse and check
 * validity of all (Data/Template Options Template) Sets in the IPFIX Message. The function
 * takes "clear" IPFIX Message wrapper (see ipx_msg_ipfix_create()) and fills reference to all Data
 * Records and Sets. Moreover, the processor also manages (Options) Templates and other auxiliary
 * structures.
 *
 * \note
 *   If a garbage message is automatically generated (for example, while an (Options) Template is
 *   removed/replaced, etc), garbage will be send during next subsequent call.
 * \note
 *   NetFlow Messages are converted to IPFIX Messages first and than processed the same way
 *   as IPFIX Messages.
 * \note
 *   Wrapper \p msg could be reallocated if it is not able to handle required amount of IPFIX Data
 *   Records.
 * \param[in]     parser Message parser
 * \param[in,out] msg    Pointer to the IPFIX Message wrapper to process (can be reallocated)
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT if the message is malformed and cannot be processed. User MUST
 *   destroy the message immediately.
 * \return #IPX_ERR_NOMEM if a memory allocation error has occurred
 */
IPX_API int
ipx_parser_process(ipx_parser_t *parser, ipx_msg_ipfix_t **msg);

/**
 * \brief Reload references to Information Elements (IE)
 *
 * Replace the current manager of IE with a new one.
 * \warning All templates will be replaced by new ones with references to the \p iemgr. This
 *   operation is very expensive!
 * \param[in] parser Message parser
 * \param[in] iemgr  Pointer to the new IE manager
 * \return #IPX_OK on success
 * \return #IPX_ERR_NOMEM if a memory allocation error has occurred
 */
IPX_API int
ipx_parser_reload_ie(ipx_parser_t *parser, fds_iemgr_t *iemgr);

/**
 * \brief Remove information about a Transport Session (TS)
 *
 * For each Observation Domain ID, the parser maintains Template Managers that are useless after
 * closing the TS. Therefore, these managers (with their templates) will be moved into
 * a garbage message that will be send immediately.
 *
 * \param[in] parser  Message parser
 * \param[in] session Transport Session to remove
 * \return #IPX_OK if the TS has been found and removed
 * \return #IPX_ERR_NOTFOUND if the TS doesn't exist in the parser
 */
IPX_API int
ipx_parser_session_remove(ipx_parser_t *parser, const struct ipx_session *session);

#endif //IPFIXCOL_PROCESSOR_H
