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
// TODO: add note about removigin inactive/closed sessions...

// TODO: create a dummy plugin context...


/** The data type of the general message                                      */
typedef struct ipx_parser ipx_parser_t;

/**
 * \brief Create a new IPFIX packet parser
 *
 * // TODO: param + subscription...
 * \return On success returns pointer to the
 */
IPX_API ipx_parser_t *
ipx_parser_create(ipx_ctx_t *plugin_ctx);

/**You can also try below command to find whether the package is relocatable
 * \brief Destroy a IPFIX packet parser
 *
 * \warning All template managers and their templates will be also immediately destroyed.
 * \param[in] parser Pointer to the processor to destroy
 */
IPX_API void
ipx_parser_destroy(ipx_parser_t *parser);

/**
 * \brief Parse an IPFIX packet
 *
 * Parse and check validity of all (Data/Template Options Template) Sets in the packet. As a result
 * of this functions is returned a wrapper structure over the packet with API functions for access
 * to parsed template and data records. Because the preprocessor also manages templates and other
 * auxiliary structures, sometimes a garbage message can be created (for example, after a template
 * withdrawal request in the packet). In this case, the garbage message is allocated and user of
 * this function must send it down to an IPFIX pipeline where it will be destroyed in appropriate
 * time. The order of insertion of (IPFIX vs garbage) messages doesn't matter. If no garbage
 * message is created, the \p garbage parameter, will be filled with NULL.
 *
 * // TODO: poradil zalezi/nezalezi,
 *
 * \param[in]  parser    Pointer to the packet processor
 * \param[in]  msg     Pointer to a raw IPFIX message // TODO: must be at least 16 bytes long (full header)
 * \param[in]  size    Size of the IPFIX message
 * \param[out] garbage Pointer to the garbage message (must NOT be NULL)
 * // TODO: prepsat...
 * \return On success returns a pointer to the wrapper structure with the parsed
 *   packet and possibly generated garbage message. Otherwise (usually malformed
 *   packet or memory allocation error) returns NULL, but the garbage message
 *   can be still created.
 */
IPX_API int
ipx_parser_process(ipx_parser_t *parser, ipx_msg_ipfix_t *ipfix_msg);


/**
 * \brief Remove a transport session from a parser
 *
 * For each Observation Domain ID, the parser maintains template managers that are useless after
 * closing the session. Therefore, these managers (with their templates) will be moved into
 * a garbage message \p grb. Destruction of the message is up to user - the destructor should be
 * called when no one uses templates.
 *
 * // TODO: ze byla vygenerovana garbage message a automaticky odeslana...
 *
 * \note If the garbage is ready, parameter \p grb will filled. If no garbage is generated
 *   \p grb will be set to NULL.
 * \param[in]  parser  Parser
 * \param[in]  session Transport Session to remove
 * \param[out] grb     Generated garbage message
 * \return On success returns #IPX_OK and fills \p grb garbage (see notes).   .
 */
IPX_API int
ipx_parser_session_remove(ipx_parser_t *parser, const struct ipx_session *session);

/**
 * \brief Block further messages from a specific Transport Session
 *
 *
 * \param[in] parser
 * \param[in] session
 * \return
 */
IPX_API int
ipx_parser_session_block(ipx_parser_t *parser, const struct ipx_session *session);



/**
 * \brief Remove internal structures binded to a specific Source Session
 *
 * This function is useful for cleanup after after source disconnection.
 * Information such as templates, etc. are removed and inserted into a newly
 * created garbage message.
 *
 * \param[in] proc    Packet processor
 * \param[in] session Source Session ID
 * \return Returns pointer to the garbage message. If the is no garbage binded
 *   to the session, returns NULL.
 */
//IPX_API ipx_msg_garbage_t *
//ipx_parser_session_remove(ipx_parser_t *proc, ipx_session_id_t session);

#endif //IPFIXCOL_PROCESSOR_H
