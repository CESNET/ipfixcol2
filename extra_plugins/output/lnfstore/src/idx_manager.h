/**
 * \file idx_manager.h
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \author Pavel Krobot <Pavel.Krobot@cesnet.cz>
 * \brief Bloom filter index for lnfstore plugin (header file)
 *
 * Copyright (C) 2016-2017 CESNET, z.s.p.o.
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

#ifndef IDX_MANAGER_H
#define IDX_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include <ipfixcol2.h>

/** Minimal false positive probability */
#define FPP_MIN (0.000001)
/** Maximal false positive probability */
#define FPP_MAX (1)

// Internal type
typedef struct idx_mgr_s idx_mgr_t;

/**
 * \brief Create a manager for a Bloom filter index
 *
 * \note
 *   Bloom filter index for current window is created in a memory. An output
 *   file for the index is opened and used in the saving phase.
 *
 * \param[in] prob     False positive probability
 * \param[in] item_cnt Projected element count (i.e. IP address count)
 * \param[in] autosize Enable automatic recalculation of parameters based on
 *   usage.
 *
 * \warning Parameter \p prob must be in range 0.000001 - 1
 * \return On success returns a pointer to the manager. Otherwise returns
 *   NULL.
 */
idx_mgr_t *
idx_mgr_create(ipx_ctx_t *ctx, double prob, uint64_t item_cnt, bool autosize);

/**
 * \brief Destroy a manager
 *
 * If an output file exits, content of the index will be stored to the file.
 * \param[in,out] index Pointer to the manager
 */
void
idx_mgr_destroy(idx_mgr_t *mgr);

/**
 * \brief Set index filename for current window
 *
 * Set a filename for future saving or loading of an index.
 *
 * \note Function makes copy of the filename string.
 * \param[in,out] index Pointer to the manager
 * \param[in]     filename Name of current index file
 * \return On success returns 0. Otherwise returns non-zero value.
 */
int
idx_mgr_set_curr_file(idx_mgr_t *mgr, char *filename);

/**
 * \brief Unset current index filename
 *
 * Free current index filename and set it to NULL.
 * \param[in,out] index Pointer to the manager
 */
void
idx_mgr_unset_curr_file(idx_mgr_t *mgr);

/**
 * \brief Store/flush an Bloom filter index to an output file
 *
 * \param[in] mgr Pointer to a manager
 * \return If save was successful or should not be done because of indexing
 *   state (i.e. "nothing to save" in the error or initial state" 0 is returned.
 *   Otherwise returns a non-zero value.
 */
int
idx_mgr_save_index(const idx_mgr_t *mgr);

/**
 * \brief Create a new window
 *
 * Each index window is stored into a file.
 * First, if a previous window exists, store the index to the previous output
 * file. Second, if automatic recalculation of parameters is enabled and
 * parameters are not suitable anymore, modify the parameters of the Bloom
 * filter index. Third, clear an internal index and prepare the new window.
 *
 * \note The output file is created after replacement by new window or by
 *   destroying its manager.
 *
 * \param[in,out] index Pointer to a manager
 * \param[in]     index_filename Name of current index file
 * \return On success returns 0. Otherwise returns non-zero value and addition
 *   of new IP addresses is not allowed.
 */
int
idx_mgr_window_new(idx_mgr_t *mgr, char *index_filename);

/**
 * \brief Close current window
 *
 * This function marks index as invalid which signalize that something is broken
 * and the next window will not be indexed.
 * \param[in,out] mgr Pointer to a manager
 */
void
idx_mgr_invalidate(idx_mgr_t *mgr);

/**
 * \brief Add an IP address to an index
 * \param[in,out] index Pointer to a manager
 * \param[in] buffer Pointer to the address stored in a buffer
 * \param[in] len    Length of the buffer
 * \return On success returns 0. Otherwise (an index window is not ready)
 *   returns non-zero value.
 */
int
idx_mgr_add(idx_mgr_t *mgr, const unsigned char *buffer, const size_t len);

#endif // IDX_MANAGER_H
