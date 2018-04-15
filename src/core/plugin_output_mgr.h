/**
 * \file src/core/plugin_output_mgr.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Internal output manager plugin (header file)
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

#ifndef IPFIXCOL_PLUGIN_OUTPUT_MGR_H
#define IPFIXCOL_PLUGIN_OUTPUT_MGR_H

#include <ipfixcol2.h>
#include "ring.h"
#include "odid_range.h"

/** Internal type of list of output destinations */
typedef struct ipx_output_mgr_list ipx_output_mgr_list_t;

/**
 * \brief Create a new output manager list
 *
 * After initialization the list is empty
 * \return Pointer or NULL (memory allocation error)
 */
ipx_output_mgr_list_t *
ipx_output_mgr_list_create();

/**
 * \brief Is the list empty?
 * \param[in] list Output manager list
 * \return True or false
 */
bool
ipx_output_mgr_list_empty(const ipx_output_mgr_list_t *list);

/**
 * \brief Destroy the list
 *
 * \note Ring buffers and ODID filters are NOT freed by this function!
 * \param[in] list Pointer or NULL (memory allocation error)
 */
void
ipx_output_mgr_list_destroy(ipx_output_mgr_list_t *list);

/**
 * \brief Add a new destination to the list
 * \param[in] list        Output manager list
 * \param[in] ring        Output plugin connection  (for a writer)
 * \param[in] odid_type   ODID filter type
 * \param[in] odid_filter ODID filter (should be NULL, if odid_type == IPX_ODID_FILTER_NONE)
 * \return #IPX_OK on success
 * \return #IPX_ERR_ARG in case of invalid combination of arguments
 * \return #IPX_ERR_NOMEM if a memory allocation error has occurred
 */
int
ipx_output_mgr_list_add(ipx_output_mgr_list_t *list, ipx_ring_t *ring,
    enum ipx_odid_filter_type odid_type, const ipx_orange_t *odid_filter);

// ------------------------------------------------------------------------------------------------

/** Description of the output manager plugin */
extern const struct ipx_plugin_info ipx_plugin_output_mgr_info;

/**
 * \brief Initialize an IPFIX parser
 * \param[in] ctx    Plugin context
 * \param[in] params Ignored (should be NULL)
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED in case of a fatal error
 */
int
ipx_plugin_output_mgr_init(ipx_ctx_t *ctx, const char *params);

/**
 * \brief Destroy an IPFIX parser
 * \note The parser is send as a garbage message before destruction.
 * \param[in] ctx Plugin context
 * \param[in] cfg Private instance data
 */
void
ipx_plugin_output_mgr_destroy(ipx_ctx_t *ctx, void *cfg);

/**
 * \brief Pass messages to output plugins
 *
 * Based on configurations (ODID filters, etc.) sets corresponding number of references and
 * passes the message.
 * \param[in] ctx Plugin context
 * \param[in] cfg Private instance data
 * \param[in] msg IPFIX or Transport Session Message to process
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED in case of a fatal error
 */
int
ipx_plugin_output_mgr_process(ipx_ctx_t *ctx, void *cfg, ipx_msg_t *msg);

#endif //IPFIXCOL_PLUGIN_OUTPUT_MGR_H
