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
#include <ipfixcol2.h>

// TODO: what IPFIX parser provides... parse messages, provides templates management,

/** Internal data type of parser                                                                 */
typedef struct ipx_parser ipx_parser_t;

/**
 * \brief Create a IPFIX parser
 *
 * \warning
 *   After initialization an IE manager is not defined and all (Options) Templates will miss
 *   definitions of elements. Use ipx_parser_ie_source() to choose the manager.
 * \param[in] ctx      Plugin context
 * \return Pointer the parser or NULL (memory allocation error)
 */
IPX_API ipx_parser_t *
ipx_parser_create(ipx_ctx_t *ctx);

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
 *   If old or no more accessible templates/template snapshots are part of a template manager,
 *   the function creates a new \p garbage message. Garbage message is generated only if processing
 *   of the Message is successful. Keep on mind, that garbage message could include templates
 *   that are referenced from the Message. Therefore, parsed message \p msg MUST be destroyed
 *   BEFORE the \p garbage.
 * \note
 *   NetFlow Messages are converted to IPFIX Messages first and than processed the same way
 *   as IPFIX Messages.
 * \note
 *   Wrapper \p msg could be reallocated if it is not able to handle required amount of IPFIX Data
 *   Records.
 * \param[in]     parser  Message parser
 * \param[in,out] ipfix     Pointer to the IPFIX Message wrapper to process (can be reallocated)
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
 * \param[in]  parser  Message parser
 * \param[in]  iemgr   Pointer to the new IE manager
 * \param[out] garbage Garbage message
 * \return #IPX_OK on success and \p garbage is defined (can be NULL, if there is no garbage)
 * \return #IPX_ERR_NOMEM if a memory allocation error has occurred, \p garbage is undefined and
 *   user MUST stop using the parser immediately! Only allowed operation is ipx_parser_destroy().
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

// Internal plugin functions -----------------------------------------------------------------------

/**
 * \brief Initialize an IPFIX parser (internal plugin)
 * \param[in] ctx    Plugin context
 * \param[in] params Ignored (should be NULL)
 * \return #IPX_OK on success
 * \return #IPX_ERR_NOMEM if a memory allocation error has occurred
 * \return #IPX_ERR_ARG in case of an internal error
 */
int
parser_plugin_init(ipx_ctx_t *ctx, const char *params);
/**
 * \brief Destroy an IPFIX parser (internal plugin)
 * \note The parser is send as a garbage message before destruction.
 * \param[in] ctx Plugin context
 * \param[in] cfg Private instance data
 */
void
parser_plugin_destroy(ipx_ctx_t *ctx, void *cfg);
/**
 * \brief Process an IPFIX or a Transport Session Message
 * \param[in] ctx Plugin context
 * \param[in] cfg Private instance data
 * \param[in] msg IPFIX or Transport Session Message to process
 * \return Always #IPX_OK or #IPX_ERR_NOMEM
 */
int
parser_plugin_process(ipx_ctx_t *ctx, void *cfg, ipx_msg_t *msg);

/**
 * \brief Prepare for an update
 *
 * Update of Information Elements will be performed during commit. It is not possible to
 * prepare a new parser because the current one can be changed before commit or abort is called.
 * \param[in] ctx    Plugin context
 * \param[in] cfg    Private instance data
 * \param[in] what   Bitwise OR of ::ipx_plugin_update flags
 * \param[in] params Ignored (should be NULL)
 * \return #IPX_READY, if the IE manager has been changed (\p what is used)
 * \return #IPX_OK otherwise
 */
int
parser_plugin_update_prepare(ipx_ctx_t *ctx, void *cfg, uint16_t what, const char *params);
/**
 * \brief Commit a modifications
 *
 * Update all (Options) Template. References to the old IE manager are replaced with new ones.
 * Old (Options) Templates are send as garbage messages.
 * \note Update can partially fail if a memory allocation error occurs during updating Template
 *   managers of Transport Sessions. These sessions will be closed or removed.
 * \param[in] ctx    Plugin context
 * \param[in] cfg    Private instance data
 * \param[in] update Private update data (made during update preparation)
 * \return #IPX_OK on success or a partial failure that is not fatal
 * \return #IPX_ERR_NOMEM if a fatal memory allocation error has occurred and the parser
 *   cannot continue.
 */
int
parser_plugin_update_commit(ipx_ctx_t *ctx, void *cfg, void *update);
/**
 * \brief Abort an update
 * \param[in] ctx    Plugin context
 * \param[in] cfg    Private instance data
 * \param[in] update Private update data (made during update preparation)
 */
void
parser_plugin_update_abort(ipx_ctx_t *ctx, void *cfg, void *update);



#endif //IPFIXCOL_PROCESSOR_H
