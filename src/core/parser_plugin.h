/**
 * \file src/core/parser_plugin.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Internal parser plugin (header file)
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

#ifndef IPFIXCOL_PARSER_PLUGIN_H
#define IPFIXCOL_PARSER_PLUGIN_H

#include <ipfixcol2.h>

/**
 * \brief Initialize an IPFIX parser
 * \param[in] ctx    Plugin context
 * \param[in] params Ignored (should be NULL)
 * \return #IPX_OK on success
 * \return #IPX_ERR_NOMEM if a memory allocation error has occurred
 * \return #IPX_ERR_ARG in case of an internal error
 */
int
parser_plugin_init(ipx_ctx_t *ctx, const char *params);

/**
 * \brief Destroy an IPFIX parser
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
 * \return #IPX_ERR_DENIED on a fatal error (such as memory allocation error, etc) The collector
 *   will exit!
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

#endif //IPFIXCOL_PARSER_PLUGIN_H
