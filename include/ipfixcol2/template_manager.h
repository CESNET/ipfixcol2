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
ipx_tmgr_create(enum ipx_session_type type);

/**
 * \brief Destroy a template manager
 * \warning The function will also immediately destroy all templates and snapshots that are stored
 *   inside the manager. If there are any references to these templates/snapshot, you must wait
 *   until you can guarantee that no one is referencing them OR you can move them into garbage
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
 * \param[in] tmgr    Template manager
 * \param[in] tl_norm Timeout of "normal" Templates (in seconds)
 * \param[in] tl_opts Timeout of Optional Templates (in seconds)
 * \return On success returns #IPX_OK. Otherwise (invalid session type) returns #IPX_ERR_ARG.
 */
IPX_API int
ipx_tmgr_set_udp_timeouts(ipx_tmgr_t *tmgr, uint16_t tl_norm, uint16_t tl_opts);

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

/**
 * \brief Add a reference to a template manager and redefine all fields
 *
 * All templates require the manager to determine a definition (a type, semantic, etc.) of
 * each template fields. If the manager is not defined or a definition of a field is missing,
 * the field cannot be properly interpreted and some information about the template are unknown.
 *
 * \warning If the manager already contains another manager, all references to definitions will be
 *   overwritten with new ones. If a definition of an IE was previously available in the older
 *   manager and the new manager doesn't include the definition, the definition will be removed
 *   and corresponding fields will not be interpretable.
 * \param[in] tmgr  Template manager
 * \param[in] iemgr Manager of Information Elements (IEs)
 * \return On success returns #IPX_OK. Otherwise returns #IPX_ERR_NOMEM and references are still
 *   the same.
 */
IPX_API int
ipx_tmgr_set_iemgr(ipx_tmgr_t *tmgr, const fds_iemgr_t *iemgr);

/**
 * \brief Set current time of a processed packet
 *
 * In a header of each IPFIX message is present so called "Export time" that helps to determine
 * a context in which it should be processed.
 *
 * In case of unreliable transmission (such as UDP, SCTP-PR), an IPFIX packet could be received
 * out of order i.e. it may be delayed. Because a scope of validity of template definitions is
 * directly connected with Export Time and the definitions can change from time to time,
 * the Export Time of the processing packet is necessary to determine to which template flow
 * records belong.
 *
 * In case of reliable transmission (such as TCP, SCTP), the Export time helps to detect wrong
 * behavior of an exporting process. For example, a TCP connection must be always reliable, thus
 * Export Time must be only increasing (i.e. the same or greater).
 *
 * \param[in] tmgr     Template manager
 * \param[in] exp_time Export time
 * \return On success returns #IPX_OK. In case of invalid behaviour (TCP only), the function will
 *   return #IPX_ERR_ARG. TODO: what next?
 */
IPX_API int
ipx_tmgr_set_time(ipx_tmgr_t *tmgr, uint32_t exp_time);

/**
 * \brief Get a reference to a template
 *
 * \warning This operation is related to the current Export Time, see ipx_tmgr_set_time()
 * \param[in] tmgr Template manager
 * \param[in] id   Identification number of the template
 * \return Pointer to the template or NULL if the template doesn't exist.
 */
IPX_API const struct ipx_template *
ipx_tmgr_template_get(const ipx_tmgr_t *tmgr, uint16_t id);

/**
 * \brief Add a template
 *
 * First, check if the new template definition doesn't break any rules for
 *
 * TODO: co bude dělat v případě toho a tam toho...
 *
 * Store the template to the template manager
 * \warning This operation is related to the current Export Time, see ipx_tmgr_set_time()
 * \param tmgr     Template manager
 * \param template Pointer to the new template
 * \return #IPX_OK if template has been successfully added/processed. If the template is not
 *   valid in this context and the session type, the function will return #IPX_ERR_ARG. We highly
 *   recommend to stop processing data of this source because this indicates invalid implementation
 *   of the exporting process.
 *   In case of memory allocation error #IPX_ERR_NOMEM. TODO: is it undefined?
 *
 */
IPX_API int
ipx_tmgr_template_add(ipx_tmgr_t *tmgr, struct ipx_template *tmplt);

// TODO flow key to template (uint64_t), jak to bude při refreshi
// TODO: jak to bude s definici vlastních poliček... tj. sekundární správce typů

IPX_API int
ipx_tmgr_template_withdraw(ipx_tmgr_t *tmgr, uint16_t id, enum ipx_template_type type);

/**
 * \brief
 *
 * \warning This operation is related to the current Export Time, see ipx_tmgr_set_time()
 * \param tmgr
 * \param type
 * \return
 */
IPX_API int
ipx_tmgr_template_withdraw_all(ipx_tmgr_t *tmgr, enum ipx_template_type type);

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
 *
 * \param
 * \param tmgr
 * \param type
 * \return
 */
IPX_API int
ipx_tmgr_template_remove_all(ipx_tmgr_t *tmgr, enum ipx_template_type type);

/**
 * TODO: lze aplikovat i na options sablony?
 * @param tmgr
 * @param id
 * @return
 */
IPX_API int
ipx_tmgr_template_set_fkey(ipx_tmgr_t *tmgr, uint16_t id);

/**
 *
 * \warning This operation is related to the current Export Time, see ipx_tmgr_set_time()
 * \param tmgr
 * \param out
 * \return
 */
IPX_API int
ipx_tmgr_snapshot_get(const ipx_tmgr_t *tmgr, const ipx_tsnapshot_t **out);

IPX_API const struct ipx_template *
ipx_tsnapshot_template_get(const ipx_tsnapshot_t *snap);

#ifdef __cplusplus
}
#endif
#endif // IPFIXCOL_TEMPLATE_MANAGER_H
