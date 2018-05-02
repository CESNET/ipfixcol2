/**
 * \file idx_manager.c
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \author Pavel Krobot <Pavel.Krobot@cesnet.cz>
 * \brief Bloom filter index for lnfstore plugin (source file)
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

// Bloomfilter index library API
#include <bf_index.h>
#include <assert.h>
#include <stdint.h>
#include "idx_manager.h"

#include <string.h> // strdup

#define BF_TOL_COEFF(x) ((x > 10000000) ? 1.1 : (x > 100000) ? 1.2 : \
    (x > 30000) ? 1.5 : (x > 5000) ? 2 : \
    (x > 500) ? 3 : 10)

/**
 * UPPPER_TOLERANCE should be small, since real unique item count should NOT
 * be higher than bloom filter estimated item count. If there are more items
 * than expected (=estimated) real false positive probability could be higher
 * than desired f. p. probability.
 */
#define BF_UPPER_TOLERANCE(val, coeff) \
    ((unsigned long)(val * (1 + coeff * 0.05)))
/**
 * LOWER_TOLERANCE could be more benevolent. In this case bloom filter size is
 * unnecessarily big. Value of LOWER_TOLERANCE depends on trade-off between
 * wasted space and frequency of bloom filter re-creation (with new
 * parameters).
 */
#define BF_LOWER_TOLERANCE(val, coeff) \
    ((unsigned long)(val * (1 + coeff * ((coeff > 1.2) ? 1.3 : 0.5) )))

/** \brief State of the manager */
enum IDX_MGR_STATE {
    IDX_MGR_S_INIT,            /**< Before creating of the first window       */
    IDX_MGR_S_WINDOW_FIRST_PARTIAL,  /**< It is expected that first window is
        * not of full window length. Thus this window is not suitable for size
        * recalculation of the next window if it has smaller record count than
        * current set up. This state is used only when the autosize of the
        * index is enabled.                                                   */
    IDX_MGR_S_WINDOW_FULL,     /**< A full length window (i.e. any consequent
        * window after the first partial window) that is suitable for size
        * recalculation of the next window.                                   */
    IDX_MGR_S_ERROR            /**< An index or output file is not ready.     */
};

/** \brief Internal structure of the manager */
struct idx_mgr_s {
    ipx_ctx_t *ctx;             /**< Instance context (only for logs!)        */
    bfi_index_ptr_t idx_ptr;    /**< Instance of a Bloom filter index         */
    char *idx_filename;     /**< Filename of current index file               */

    struct {
        uint64_t est_items; /**< Estimated item count in a Bloom filter       */
        double   fp_prob;   /**< False positive probability of a Bloom filter */
    } cfg_bloom;            /**< Configuration of a Bloom Filter              */

    struct {
        bool  en_autosize;        /**< Enable auto-size (on/off)              */
        enum IDX_MGR_STATE state; /**< State of the manager                   */
    } cfg_mgr;             /**< Configuration of the manager                  */
};


idx_mgr_t *
idx_mgr_create(ipx_ctx_t *ctx, double prob, uint64_t item_cnt, bool autosize)
{
    // Check parameters
    if (prob < FPP_MIN || prob > FPP_MAX) {
        IPX_CTX_ERROR(ctx, "Index manager error (the probability parameter is out of range).", '\0');
        return NULL;
    }

    // Create structures
    idx_mgr_t *mgr = (idx_mgr_t *) calloc(1, sizeof(idx_mgr_t));
    if (!mgr) {
        // Memory allocation failed
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        return NULL;
    }

    // Save parameters
    mgr->cfg_bloom.est_items = item_cnt;
    mgr->cfg_bloom.fp_prob = prob;
    mgr->cfg_mgr.en_autosize = autosize;
    mgr->cfg_mgr.state = IDX_MGR_S_INIT;
    mgr->ctx = ctx;
    return mgr;
}

void
idx_mgr_destroy(idx_mgr_t *mgr)
{
    if (!mgr) {
        return;
    }

    idx_mgr_save_index(mgr);
    if (mgr->idx_ptr) {
        bfi_destroy_index(&(mgr->idx_ptr));
    }

    free(mgr->idx_filename);
    free(mgr);
}

int
idx_mgr_set_curr_file(idx_mgr_t *mgr, char *filename)
{
    assert(mgr->idx_filename == NULL);

    mgr->idx_filename = strdup(filename);
    if (!mgr->idx_filename) {
        IPX_CTX_ERROR(mgr->ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        return 1;
    }

    return 0;
}

void
idx_mgr_unset_curr_file(idx_mgr_t *mgr)
{
    free(mgr->idx_filename);
    mgr->idx_filename = NULL;
}

int
idx_mgr_save_index(const idx_mgr_t *mgr)
{
    bfi_ecode_t ret;

    if (mgr->cfg_mgr.state != IDX_MGR_S_WINDOW_FULL &&
            mgr->cfg_mgr.state != IDX_MGR_S_WINDOW_FIRST_PARTIAL) {
        // Index file is broken or doesn't exist, don't save.
        return 0;
    }

    ret = bfi_store_index(mgr->idx_ptr, mgr->idx_filename);
    if (ret != BFI_E_OK) {
        IPX_CTX_ERROR(mgr->ctx, "Failed to store a BF index: %s", bfi_get_error_msg(ret));
        return 1;
    }

    return 0;
}

/**
 * \brief Prepare the Bloom filter index
 *
 * Create & initialize a new Bloom filter index. If previous one still exists,
 * it is destroyed first.
 * \param[in,out] mgr Pointer to an index manager
 * \return On success returns 0. Otherwise returns a non-zero value.
 */
static int
idx_mgr_index_prepare(idx_mgr_t *mgr)
{
    bfi_ecode_t ret;

    if (mgr->idx_ptr != NULL) {
        // Destroy previous instance
        bfi_destroy_index(&(mgr->idx_ptr));
    }

    bfi_index_ptr_t new_index;
    ret = bfi_init_index(&new_index, mgr->cfg_bloom.est_items,
                            mgr->cfg_bloom.fp_prob);
    if (ret != BFI_E_OK) {
        IPX_CTX_ERROR(mgr->ctx, "Failed to initialize a BF index: %s", bfi_get_error_msg(ret));
        return 1;
    }

    mgr->idx_ptr = new_index;
    return 0;
}

int
idx_mgr_window_new(idx_mgr_t *mgr, char *index_filename)
{
    bool reinit = false;
    bfi_ecode_t ret;

    idx_mgr_unset_curr_file(mgr);

    // Check indexing state
    if (mgr->cfg_mgr.state == IDX_MGR_S_INIT ||
            mgr->cfg_mgr.state == IDX_MGR_S_ERROR) {
        reinit = true;
    }

    if (!reinit && mgr->cfg_mgr.en_autosize) {
        /*
         * Calculate minimal & maximal expected estimate (item count in Bloom
         * filter index) based on number of elements in the current window.
         */
        uint64_t act_cnt = bfi_stored_item_cnt(mgr->idx_ptr);
        double coeff = BF_TOL_COEFF(act_cnt);

        double est_low = BF_LOWER_TOLERANCE(act_cnt, coeff);
        double est_high = BF_UPPER_TOLERANCE(act_cnt, coeff);

        /*
         * Compare the current configuration of the index estimate and the
         * expected (low and high) estimates.
         */
        if (est_high > mgr->cfg_bloom.est_items) {
            // Higher act_cnt = make bigger bloom filter
            mgr->cfg_bloom.est_items = act_cnt * coeff;
            reinit = true;
        } else if (est_low < mgr->cfg_bloom.est_items && act_cnt > 0 &&
                mgr->cfg_mgr.state == IDX_MGR_S_WINDOW_FULL) {
            // Lower act_cnt -> save space, make smaller bloom filter
            // Note: allow size reduction only based on the FULL previous window
            mgr->cfg_bloom.est_items = act_cnt * coeff;
            reinit = true;
        }
    }

    // Prepare index
    if (reinit) {
        // Destroy & create a new index (new parameters)
        if (idx_mgr_index_prepare(mgr) != 0) {
            // Something went wrong
            idx_mgr_invalidate(mgr);
            return 1;
        }
    } else {
        // Only clear the current index (parameters are the same)
        ret = bfi_clear_index(mgr->idx_ptr);
        if (ret != BFI_E_OK){
            IPX_CTX_ERROR(mgr->ctx, "Failed to clean a BF index: %s", bfi_get_error_msg(ret));
            idx_mgr_invalidate(mgr);
            return 1;
        }
    }

    if (idx_mgr_set_curr_file(mgr, index_filename) != 0){
        // Something went wrong
        idx_mgr_invalidate(mgr);
        return 1;
    }

    // Change state of the manager
    switch (mgr->cfg_mgr.state) {
    case IDX_MGR_S_INIT:
        mgr->cfg_mgr.state = (mgr->cfg_mgr.en_autosize)
            ? IDX_MGR_S_WINDOW_FIRST_PARTIAL
            : IDX_MGR_S_WINDOW_FULL;
        break;

    case IDX_MGR_S_WINDOW_FIRST_PARTIAL:
        mgr->cfg_mgr.state = IDX_MGR_S_WINDOW_FULL;
        break;

    case IDX_MGR_S_WINDOW_FULL:
        // Do not change the state
        break;

    case IDX_MGR_S_ERROR:
        // Recovery from an error can occur only with start of a new window
        mgr->cfg_mgr.state = IDX_MGR_S_WINDOW_FULL;
        break;
    }

    return 0;
}

void
idx_mgr_invalidate(idx_mgr_t *mgr)
{
    mgr->cfg_mgr.state = IDX_MGR_S_ERROR;
}

int
idx_mgr_add(idx_mgr_t *mgr, const unsigned char *buffer, const size_t len)
{
    bfi_ecode_t ret;

    if (mgr->cfg_mgr.state != IDX_MGR_S_WINDOW_FULL &&
            mgr->cfg_mgr.state != IDX_MGR_S_WINDOW_FIRST_PARTIAL) {
        return 1;
    }

    ret = bfi_add_addr_index(mgr->idx_ptr, buffer, len);
    if (ret != BFI_E_OK) {
        IPX_CTX_ERROR(mgr->ctx, "Failed to add a record to a BF index: %s", bfi_get_error_msg(ret));
        idx_mgr_invalidate(mgr);
        return 1;
    }

    return 0;
}

