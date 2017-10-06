/**
 * \file   include/ipfixcol2/template_manager.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief  Template manager (header file)
 * \date   October 2017
 */

/*
 * Copyright (C) 2017 CESNET, z.s.p.o.
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

#ifndef IPFIXCOL_TEMPLATE_MANAGER_H
#define IPFIXCOL_TEMPLATE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <libfds.h>
#include <ipfixcol2/api.h>
#include "message_garbage.h"
#include "source.h"
#include "template.h"


/** Internal template manager declaration   */
typedef struct ipx_tmgr ipx_tmgr_t;
/** Internal template snapshot declaration  */
typedef struct ipx_tsnapshot ipx_tsnapshot_t;

// TODO: describe how it works, something about snapshots etc.

/**
 * \brief Create a new template manager
 *
 * This manager is able to handle templates that belong to a combination of the Transport Session
 * and Observation Domain. To configure allowed and prohibited behavior of template (re)definition
 * and withdrawing, the type of session must be configured.
 * \note In case of UDP session, Template and Option Template timeouts should be configured using
 *   ipx_tmgr_set_udp_timeouts() function. By default, the timeouts are disabled.
 * \param[in] type Session type
 * \return Pointer to the manager or NULL (usually memory allocation error)
 */
IPX_API ipx_tmgr_t *
ipx_tmgr_create(enum IPX_SESSION_TYPE type);

/**
 * \brief Destroy a template manager
 * \warning The function will also immediately destroy all templates and snapshots that are stored
 *   inside the manager. If there are any references to these templates/snapshot, you must wait
 *   until you can guarantee that no one is referencing them or you can move them into garbage
 *   (see ipx_tmgr_clear()) and then generate a garbage message that must be later destroyed
 *   (see ipx_tmgr_garbage_get()). In the latter case, you can safely destroy the manager, but
 *   the garbage must remain until references exist.
 * \param[in] tmgr Template manager
 */
IPX_API void
ipx_tmgr_destroy(ipx_tmgr_t *tmgr);

/**
 * \brief Move all valid templates and snapshots to garbage
 *
 * After cleaning, the template manager will be the same as a newly create manager, except the
 * configuration parameters (timeouts, IE manager, etc.) will be preserved.
 * \note Garbage can be retrieved using ipx_tmgr_garbage_get() function.
 * \param[in] tmgr Template manager
 */
IPX_API void
ipx_tmgr_clear(ipx_tmgr_t *tmgr);

/**
 * \brief Collect internal garbage and create it as garbage message
 *
 * All unreachable (or old) templates and snapshots will be moved into the internal garbage
 * collection that will be returned in form of a garbage message.
 * \param[in] tmgr Template manager
 * \return On success returns a pointer to the garbage or NULL, if there is nothing to throw out.
 */
IPX_API ipx_garbage_msg_t *
ipx_tmgr_garbage_get(ipx_tmgr_t *tmgr);

/**
 * \brief Set (Options) Template lifetime (UDP session only)
 *
 * (Options) Templates that are not received again (i.e. not refreshed by the Exporting Process
 * or someone else) within the configured lifetime become invalid and then automatically
 * discarded (moved to garbage) by the manager. All timeout are related to Export Time
 * (see ipx_tmgr_set_time()).
 *
 * \note To disable timeout, use value 0. In this case, templates exists throughout the whole
 *   existence of the manager or until they are redefined/updated by another template with the
 *   same ID.
 * \param[in] tmgr         Template manager
 * \param[in] template     Timeout of "normal" Templates (in seconds)
 * \param[in] opt_template Timeout of Optional Templates (in seconds)
 * \return On success returns #IPX_OK. Otherwise (invalid session type) returns #IPX_ERR_ARG.
 */
IPX_API int
ipx_tmgr_set_udp_timeouts(ipx_tmgr_t *tmgr, uint16_t template, uint16_t opt_template);

/**
 * \brief Set timeout of template snapshots
 *
 * TODO: Validity range ... rozsah platnosti kazdeho snapshotu je dan zacatkem platnosti a casem
 * konce platnosti... tj. cas kdy byl nahrazen novým. Pokud tedy koncovy cas snapshotu je
 * mensi nez nastaveny aktualni cas o hodnotu timeoutu, je povazovan za zastaraly a vyhozen do
 * odpadu...
 *
 * \warning High values have a significant impact on performance and memory consumption.
 *   The recommended range of the timeout value is 0 - 60.
 * \param[in] tmgr    Template manager
 * \param[in] timeout Timeout (in seconds)
 */
IPX_API void
ipx_tmgr_set_snapshot_timeout(ipx_tmgr_t *tmgr, uint16_t timeout);


IPX_API int
ipx_tmgr_set_iemgr(ipx_tmgr_t *tmgr, const fds_iemgr_t *iemgr);


IPX_API int
ipx_tmgr_set_time(ipx_tmgr_t *tmgr, uint32_t exp_time);

/**
 * \brief
 *
 * \warning This operation is related to the current Export Time, see ipx_tmgr_set_time()
 * \param tmgr
 * \param id
 * \return
 */
IPX_API const struct ipx_template *
ipx_tmgr_template_get(ipx_tmgr_t *tmgr, uint16_t id);

/**
 * \brief
 *
 * \warning This operation is related to the current Export Time, see ipx_tmgr_set_time()
 * \param tmgr
 * \param template
 * \return
 */
IPX_API int
ipx_tmgr_template_add(ipx_tmgr_t *tmgr, struct template *template);

// TODO flow key to template (uint64_t)

/**
 * \brief
 *
 * \warning This operation is related to the current Export Time, see ipx_tmgr_set_time()
 * \param tmgr
 * \param id
 * \param type
 * \return
 */
IPX_API int
ipx_tmgr_template_remove(ipx_tmgr_t *tmgr, uint16_t id, enum ipx_template_type type);

/**
 * \brief
 *
 * \warning This operation is related to the current Export Time, see ipx_tmgr_set_time()
 * \param
 * \param tmgr
 * \param type
 * \return
 */
IPX_API int
ipx_tmgr_template_remove_group(ipx_tmgr_t *tmgr, enum ipx_template_type type);

/**
 *
 * \warning This operation is related to the current Export Time, see ipx_tmgr_set_time()
 * \param tmgr
 * \param out
 * \return
 */
IPX_API int
ipx_tmgr_snapshot_get(ipx_tmgr_t *tmgr, const ipx_tsnapshot_t **out);


IPX_API const struct *
ipx_tsnapshot_template_get(const ipx_tsnapshot_t *snap);

#ifdef __cplusplus
}
#endif
#endif // IPFIXCOL_TEMPLATE_MANAGER_H
