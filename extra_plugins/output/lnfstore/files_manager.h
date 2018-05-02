/**
 * \file files_manager.c
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Output files manager (source file)
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

#ifndef LNFSTORAGE_FILES_H
#define LNFSTORAGE_FILES_H

#include <libnf.h>
#include <ipfixcol2.h>

#include <time.h>
#include <inttypes.h>
#include <stdbool.h>

typedef struct files_mgr_s files_mgr_t;

/** Output files */
enum FILES_MODE {
    FILES_M_LNF   = 0x1,  /**< LNF file (nfdump)        */
    FILES_M_INDEX = 0x2,  /**< Bloom filter index       */
    FILES_M_ALL   = 0x3   /**< Create all above files (binary OR !!!)   */
};

/**
 * \brief Template for output files
 *
 * Output files is stored based on following templates:
 * \n LNF file:   'dir'/YY/MM/DD/'prefixes.lnf''suffix_mask'
 * \n Index file: 'dir'/YY/MM/DD/'prefixes.bloom''suffix_mask'
 *
 * Where /YY/MM/DD means year/month/day. Variables 'dir' and 'prefixes.xxx'
 * should not include any special format characters for date & time.
 * Variable 'suffix_mask' MUST include special format characters (at least,
 * for time) that are replaced with appropriate date & time representation!
 *
 * \note
 *   All fields except 'prefixes.xxx' are mandatory i.e. MUST NOT be NULL.
 *   'prefixes.lnf' and 'prefixes.bool' MUST NOT be equal, if indexing is turned
 *   on, at least one of 'prefixes.xxx' have to be set.
 */
struct files_mgr_paths {
    const char *dir;            /**< Storage directory */

    /**
     * \brief File prefixes
     * \note If any variable is not used, it can be defined as NULL.
     */
    struct {
        const char *lnf;     /**< File prefix of a "LNF" file                */
        const char *index;   /**< File prefix of a "Bloom filter index" file */
    } prefixes;

    const char *suffix_mask;    /**< File mask */
};

/**
 * \brief Structure for Bloom filter index parameters
 */
struct files_mgr_idx_param {
    double   prob;       /**< False positive probability */
    uint64_t item_cnt;   /**< Projected element count (i.e. IP address count) */
    bool     autosize;   /**< Enable automatic recalculation of parameters
                           *  based on usage. */
};

/**
 * \brief Structure for LNF file parameters
 */
struct files_mgr_lnf_param {
    bool compress;       /**< Enable LNF file compression                     */
    const char *ident;   /**< File identifier for newly create files
                           *  (can be set to NULL)                            */
};

/**
 * \brief Create an output file manager
 *
 * \note
 *   Output files for current window do not exist after creation of the output
 *   file manager. Adding records to output files without the window
 *   configuration causes an error. To create a new window and it's storage
 *   files use files_mgr_new_window().
 *
 * \param[in] ctx       Instance context (only for logs!)
 * \param[in] mode      Type of output file(s)
 * \param[in] paths     Path template of output file(s)
 * \param[in] lnf_param Parameters for LNF storage (can be NULL if the
 *                      corresponding output file is not enabled)
 * \param[in] idx_param Parameters for Bloom Filter index (can be NULL if the
 *                      corresponding output file is not enabled)
 * \return On success returns a pointer to the manager. Otherwise returns NULL.
 */
files_mgr_t *
files_mgr_create(ipx_ctx_t *ctx, enum FILES_MODE mode, const struct files_mgr_paths *paths,
    const struct files_mgr_lnf_param *lnf_param, const struct files_mgr_idx_param *idx_param);

/**
 * \brief Destroy an output file manager
 *
 * If output files exist, their (buffered) content is flushed to the files.
 * \param[in,out] mgr Pointer to the manager
 */
void
files_mgr_destroy(files_mgr_t *mgr);

/**
 * \brief Create a new window
 *
 * First, if a previous window exists, flush and close all opened output files.
 * Second, create output files.
 *
 * \param[in, out] mgr Pointer to a manager
 * \param[in]      ts  Start timestamp of the window
 * \return On success returns 0. Otherwise (failed to create any of output
 *   files/sub-managers) returns a non-zero value.
 *
 * \note Even if the function failed to create one or more output files, other
 *   successfully initialized output files are ready to use.
 */
int
files_mgr_new_window(files_mgr_t *mgr, const time_t *ts);

/**
 * \brief Disable all outputs
 *
 * If a previous window exists, flush and close all opened output files.
 * The outputs will be in invalid states, therefore files_mgr_add_record()
 * will always return an error code.
 *
 * To enable the outputs simply call files_mgr_new_window().
 */
void
files_mgr_invalidate(files_mgr_t *mgr);

/**
 * \brief Add a record to output files
 *
 * \param[in,out] mgr     File manager
 * \param[in]     rec_ptr Pointer to the record
 * \return On success returns 0. Otherwise (failed to write to any of output
 *   files) returns a non-zero value.
 */
int
files_mgr_add_record(files_mgr_t *mgr, lnf_rec_t *rec_ptr);

/**
 * \brief Get the storage directory of the manager
 * \return The directory
 */
const char *
files_mgr_get_storage_dir(files_mgr_t *mgr);

/**
 * \brief Remove unwanted characters in a path (auxiliary function)
 *
 * This function removes multiple slashes in the path. The size of the path
 * can be only reduced, so all operations are performed in-situ.
 * \param[in,out] path Path
 */
void
files_mgr_names_sanitize(char *path);

#endif //LNFSTORAGE_FILES_H
