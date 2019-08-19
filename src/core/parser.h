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

#include <ipfixcol2/message.h>
#include <ipfixcol2/verbose.h>

/**
 * \defgroup ipxParser IPFIX Message parser
 * \brief Parse IPFIX Messages and handle Template managers of Transport Sessions
 *
 * The parser takes clean IPFIX Message wrapper, checks Message consistency, parser Data and
 * (Options) Template Sets and and fills positions of Data Records and references to particular
 * templates necessary to interpret them. For each combination of a Transport Session and an ODID,
 * the parser manages a Template manger and expected sequence numbers.
 *
 * Keep on mind that all referenced templates are part of Template Managers and they are part of
 * the parser. In other words, the parser MUST not be destroyed until all records (that have
 * references to its templates) no longer exist.
 *
 * @{
 */

/** Internal data type of parser                                                                 */
typedef struct ipx_parser ipx_parser_t;

/**
 * \brief Create a IPFIX parser
 *
 * \warning
 *   After initialization an IE manager is not defined and all (Options) Templates will miss
 *   definitions of elements. Use ipx_parser_ie_source() to choose the manager.
 * \param[in] ident  Identification
 * \param[in] vlevel Verbosity level
 * \return Pointer the parser or NULL (memory allocation error)
 */
IPX_API ipx_parser_t *
ipx_parser_create(const char *ident, enum ipx_verb_level vlevel);

/**
 * \brief Destroy an IPFIX parser
 *
 * \warning All template managers and their templates will be also immediately destroyed.
 * \param[in] parser Message parser
 */
IPX_API void
ipx_parser_destroy(ipx_parser_t *parser);

/**
 * \brief Change verbosity level
 *
 * If \p v_new is non-NULL, the new level is set from \p v_new.
 * If \p v_old is non-NULL, the previous level is saved in \p v_old.
 * \param[in] parser Message Parser
 * \param[in] v_new  New verbosity level (can be NULL)
 * \param[in] v_old  Old verbosity level (can be NULL)
 */
IPX_API void
ipx_parser_verb(ipx_parser_t *parser, enum ipx_verb_level *v_new, enum ipx_verb_level *v_old);

/**
 * \brief Process IPFIX (or NetFlow) Message
 *
 * The function takes "clear" IPFIX Message wrapper (see ipx_msg_ipfix_create()) and fills
 * reference to all Data Records and Sets.
 * First, the functions tries to find information about a Transport Session (TS) and ODID of
 * the Message which holds a Template manager and expected sequence number of the Message.
 * If this is the first record from the TS and ODID, new info is created.
 * Second, if the message is in the form of NetFlow, it is converted to IPFIX.
 * Finally, it parses and checks validity of all (Data/Template Options Template) Sets in the
 * IPFIX Message.
 *
 * \note
 *   If old or no more accessible templates/template snapshots are part of a template manager,
 *   the function creates a new \p garbage message. Garbage message is generated only if processing
 *   of the Message is successful. Keep on mind, that garbage message could include templates
 *   that are referenced from the Message. Therefore, parsed message \p msg MUST be destroyed
 *   BEFORE the \p garbage.
 * \note
 *   Wrapper \p msg could be reallocated if it is not able to handle required amount of IPFIX Data
 *   Records.
 * \param[in]     parser  Message parser
 * \param[in,out] ipfix   Pointer to the IPFIX Message wrapper to process (can be reallocated)
 * \param[out]    garbage Pointer to the garbage message with old templates, snapshots, etc.
 * \return #IPX_OK on success and fills a pointer to the \p garbage message. If the value is NULL,
 *   no garbage is available.
 * \return #IPX_ERR_FORMAT if the message is malformed and cannot be processed. User MUST
 *   destroy the message immediately and the Session should be closed.
 * \return #IPX_ERR_NOMEM if a memory allocation error has occurred. User MUST destroy the message
 *   and the Session should be closed.
 * \return #IPX_ERR_DENIED if a Transport Session has been blocked by ipx_parser_session_block()
 */
IPX_API int
ipx_parser_process(ipx_parser_t *parser, ipx_msg_ipfix_t **ipfix, ipx_msg_garbage_t **garbage);

/**
 * \brief Set source of Information Elements (IE)
 *
 * Replace a pointer to the current manager of IE with a new one. The manager MUST exists at least
 * until the parser is destroyed or the manager is replaced. Keep on mind that all Data Records and
 * their (Options) Templates processed by the parser still have references to the manager.
 * Therefore, the IE manager cannot be freed until all records are destroyed.
 *
 * \warning
 *   All templates will be replaced by new ones with references to the \p iemgr. Therefore, this
 *   operation is very expensive if the parser is not empty!
 * \warning
 *   In case of memory allocation error, state of internal template managers is undefined.
 *   Connection with Transport Sessions MUST be immediately closed or removed! By default, all
 *   Transport Sessions are blocked (equivalent to calling ipx_parser_session_block() on all
 *   Transport Sessions). User MUST destroy the parser or remove all Transport Sessions
 *   using ipx_parser_session_remove() within ipx_parser_session_for().
 *
 * \param[in]  parser  Message parser
 * \param[in]  iemgr   Pointer to the new IE manager
 * \param[out] garbage Garbage message
 * \return #IPX_OK on success and \p garbage is defined (can be NULL, if there is no garbage)
 * \return #IPX_ERR_NOMEM if a memory allocation error has occurred, \p garbage is undefined and
 *   user MUST stop using the parser immediately! See notes.
 */
IPX_API int
ipx_parser_ie_source(ipx_parser_t *parser, const fds_iemgr_t *iemgr, ipx_msg_garbage_t **garbage);

/**
 * \brief Remove information about a Transport Session (TS)
 *
 * For each Observation Domain ID, the parser maintains Template Managers that are useless after
 * closing the TS. Therefore, these managers (with their templates) will be moved into
 * a garbage message that MUST be destroy later by a user when there are no more references to
 * the old template of the TS.
 *
 * \param[in]  parser  Message parser
 * \param[in]  session Transport Session to remove
 * \param[out] garbage Garbage message with Template manager, etc.
 * \return #IPX_OK if the TS has been found and \p garbage is set. In case of memory allocation
 *   failure, the value is NULL and garbage is lost!
 * \return #IPX_ERR_NOTFOUND if the TS doesn't exist in the parser. \p garbage is undefined.
 */
IPX_API int
ipx_parser_session_remove(ipx_parser_t *parser, const struct ipx_session *session,
    ipx_msg_garbage_t **garbage);

/**
 * \brief Ignore processing of messages that corresponds to a Transport Session (TS)
 *
 * All Observation Domain IDs will be blocked until the Transport Session is removed
 * (see ipx_parser_session_remove()).
 * \param[in] parser  Parser
 * \param[in] session Transport Session to block
 * \return #IPX_OK on success
 * \return #IPX_ERR_NOTFOUND if the TS is not present
 */
IPX_API int
ipx_parser_session_block(ipx_parser_t *parser, const struct ipx_session *session);

/**
 * \brief For loop callback function
 *
 * It's safe to call ipx_parser_session_block() and ipx_parser_session_remove() functions in
 * the callback function on current Transport Session.
 * \param[in] parser Parser
 * \param[in] ts     Transport Session
 * \param[in] data   User defined data
 */
typedef void (*ipx_parser_for_cb)(ipx_parser_t *parser, const struct ipx_session *ts, void *data);

/**
 * \brief Call a function for each Transport Session in the parser
 *
 * \param parser Parser
 * \param cb     Callback function
 * \param data   User defined data
 */
IPX_API void
ipx_parser_session_for(ipx_parser_t *parser, ipx_parser_for_cb cb, void *data);

/**
 * @}
 */

#endif //IPFIXCOL_PROCESSOR_H
