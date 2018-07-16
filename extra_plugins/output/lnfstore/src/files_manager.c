/**
 * \file files_manager.c
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \author Pavel Krobot <Pavel.Krobot@cesnet.cz>
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

#include <string.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "files_manager.h"
#include "idx_manager.h"
#include "utils.h"

// Internal representation of output files
struct files_mgr_s {
    /** Output files or managers */
    struct {
        lnf_file_t *file_lnf;     /**< LNF file     */
        idx_mgr_t  *index_mgr;    /**< Bloom filter index manager (contains
                                    *  index output file) */
    } outputs;

    /** Copy of output templates */
    struct files_mgr_paths *paths_tmplt;

    /** LNF compression */
    struct {
        bool compress;  /**< Enable compression            */
        char *ident;    /**< File identifier (can be NULL) */
    } lnf_params;

    /** Files to create/update                             */
    enum FILES_MODE mode;
    /** Instance context (only for logs!)                  */
    ipx_ctx_t *ctx;
};

/**
 * \brief Structure for generated filenames and directories
 */
struct files_mgr_names {
    char *dir;         /**< Directory              */
    char *file_lnf;    /**< LNF file (full path)   */
    char *file_index;  /**< Index file (full path) */
};

/**
 * \brief Check a template for output files
 *
 * Check if all required fields are filled (i.e. not NULL) and if a combination
 * of output files and a path template cannot cause filename collisions
 * in the same window (i.e. the same name of files).
 *
 * \param[in] ctx    Instance context (only for logs!)
 * \param[in] mode   Type of output file(s)
 * \param[in] paths  Path template for output files
 * \return On success returns 0. Otherwise returns a non-zero value.
 */
static int
files_mgr_path_check(ipx_ctx_t *ctx, enum FILES_MODE mode, const struct files_mgr_paths *paths)
{
    // Check required fields
    if (!paths->dir) {
        IPX_CTX_ERROR(ctx, "File manager error (output directory is not defined).", '\0');
        return 1;
    }

    if (!paths->suffix_mask) {
        IPX_CTX_ERROR(ctx, "File manager error (suffix mask is not defined).", '\0');
        return 1;
    }

    // Check prefix collision
    int n_files = 0;
    int n_empty_prefix = 0;

    if (mode & FILES_M_LNF) {
        n_files++;

        if (!paths->prefixes.lnf || strlen(paths->prefixes.lnf) == 0) {
            n_empty_prefix++;
        }
    }

    if (mode & FILES_M_INDEX) {
        n_files++;

        if (!paths->prefixes.index || strlen(paths->prefixes.index) == 0) {
            n_empty_prefix++;
        }
    }

    if (n_files <= 1) {
        // Only one input file -> no collisions in the same window
        return 0;
    }

    if (n_empty_prefix > 1) {
        // Two empty prefixes -> collision
        IPX_CTX_ERROR(ctx, "File manager error (missing file prefixes cause filename collision).",
            '\0');
        return 1;
    }

    // Check prefix names
    if (n_empty_prefix == 0 && strcmp(paths->prefixes.lnf, paths->prefixes.index) == 0) {
        // The same prefixes -> collision
        IPX_CTX_ERROR(ctx, "File manager error (the same file prefix for LNF and Index file is "
            "not allowed).", '\0');
        return 1;
    }

    // Everything seems to be OK
    return 0;
}

/**
 * \brief Delete (i.e. free) a copy of a path template
 * \param paths Pointer to the copy
 */
static void
files_mgr_path_free(struct files_mgr_paths *paths)
{
    if (!paths) {
        return;
    }

    if (paths->dir) {
        free((void *)paths->dir);
    }

    if (paths->suffix_mask) {
        free((void *)paths->suffix_mask);
    }

    if (paths->prefixes.lnf) {
        free((void *)paths->prefixes.lnf);
    }

    if (paths->prefixes.index) {
        free((void *)paths->prefixes.index);
    }

    free(paths);
}

/**
 * \brief Make a copy of a path template
 * \warning The structure MUST be freed by files_mgr_path_free()
 * \param paths Pointer to original structure
 * \return On success returns a pointer to the copy. Otherwise (usually memory
 *   allocation error) returns NULL.
 */
static struct files_mgr_paths *
files_mgr_path_copy(const struct files_mgr_paths *paths)
{
    if (!paths) {
        return NULL;
    }

    // Create a new structure
    struct files_mgr_paths *cpy;
    cpy = (struct files_mgr_paths *) calloc(1, sizeof(struct files_mgr_paths));
    if (!cpy) {
        return NULL;
    }

    // Copy the directory
    cpy->dir = strdup(paths->dir);
    if (!cpy->dir) {
        files_mgr_path_free(cpy);
        return NULL;
    }

    // Copy the suffix mask
    cpy->suffix_mask = strdup(paths->suffix_mask);
    if (!cpy->suffix_mask) {
        files_mgr_path_free(cpy);
        return NULL;
    }

    // Copy the prefixes
    if (paths->prefixes.lnf) {
        cpy->prefixes.lnf = strdup(paths->prefixes.lnf);
        if (!cpy->prefixes.lnf) {
            files_mgr_path_free(cpy);
            return NULL;
        }
    }

    if (paths->prefixes.index) {
        cpy->prefixes.index = strdup(paths->prefixes.index);
        if (!cpy->prefixes.index) {
            files_mgr_path_free(cpy);
            return NULL;
        }
    }

    return cpy;
}

files_mgr_t *
files_mgr_create(ipx_ctx_t *ctx, enum FILES_MODE mode, const struct files_mgr_paths *paths,
    const struct files_mgr_lnf_param *lnf_param, const struct files_mgr_idx_param *idx_param)
{
    // Check parameters
    if (!(mode & FILES_M_ALL)) {
        // No output file -> error
        IPX_CTX_ERROR(ctx, "File manager error (no output files enabled).");
        return NULL;
    }

    if (files_mgr_path_check(ctx, mode, paths)) {
        return NULL;
    }

    if (mode & FILES_M_LNF && lnf_param == NULL) {
        IPX_CTX_ERROR(ctx, "File manager error (missing parameters for LNF storage).", '\0');
        return NULL;
    }

    if (mode & FILES_M_INDEX && idx_param == NULL) {
        IPX_CTX_ERROR(ctx, "File manager error (missing parameters for Bloom filter index).", '\0');
        return NULL;
    }

    // Create an internal structure
    files_mgr_t *mgr = (files_mgr_t *) calloc(1, sizeof(files_mgr_t));
    if (!mgr) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        return NULL;
    }

    mgr->paths_tmplt = files_mgr_path_copy(paths);
    if (!mgr->paths_tmplt) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        free(mgr);
        return NULL;
    }

    // Configure Bloom filter index
    if (mode & FILES_M_INDEX) {
        // Create file index (IDX) manager
        mgr->outputs.index_mgr = idx_mgr_create(ctx, idx_param->prob, idx_param->item_cnt,
            idx_param->autosize);
        if (!mgr->outputs.index_mgr) {
            IPX_CTX_ERROR(ctx, "Files manager error (unable to create index manager).", '\0');
            files_mgr_path_free(mgr->paths_tmplt);
            free(mgr);
            return NULL;
        }
    }

    // Configure LNF file
    if (mode & FILES_M_LNF) {
        // Copy LNF configuration
        mgr->lnf_params.compress = lnf_param->compress;
        if (lnf_param->ident) {
            mgr->lnf_params.ident = strdup(lnf_param->ident);
            if (!mgr->lnf_params.ident) {
                IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
                idx_mgr_destroy(mgr->outputs.index_mgr);
                files_mgr_path_free(mgr->paths_tmplt);
                free(mgr);
                return NULL;
            }
        }
    }

    mgr->ctx = ctx;
    mgr->mode = mode & FILES_M_ALL;
    return mgr;
}

void
files_mgr_destroy(files_mgr_t *mgr)
{
    // Close/flush output all files
    if (mgr->outputs.file_lnf) {
        lnf_close(mgr->outputs.file_lnf);
    }

    if (mgr->outputs.index_mgr) {
        idx_mgr_destroy(mgr->outputs.index_mgr);
    }

    // Delete path template
    files_mgr_path_free(mgr->paths_tmplt);

    // Delete an identifier of new files
    free(mgr->lnf_params.ident);

    // Destroy the internal structure
    free(mgr);
}

/**
 * \brief Allocate memory and concatenate filename parts
 *
 * \param[in] dir    Directory
 * \param[in] prefix File prefix
 * \param[in] suffix File suffix
 *
 * \note If any of parameters (even all) is NULL, the parameter is replaced
 *   with empty string i.e. "".
 * \note If the last character of \p dir is not '/' than this character is
 *   automatically added. Except situation when \p dir is NULL.
 * \warning Returned pointer must be freed manually (standard free)
 * \return On success returns a pointer to the filename. Otherwise returns NULL.
 */
static char *
files_mgr_names_create_aux(const char *dir, const char *prefix, const char *suffix)
{
    size_t len_dir =    (dir != NULL)    ? strlen(dir)    : 0;
    size_t len_prefix = (prefix != NULL) ? strlen(prefix) : 0;
    size_t len_suffix = (suffix != NULL) ? strlen(suffix) : 0;
    size_t len_total = len_dir + len_prefix + len_suffix + 2; // 2 == '/' + '\0'
    const char *empty = "";

    // Check file length
    if (len_total > PATH_MAX) {
        return NULL;
    }

    // Replace NULL variables with ""
    bool add_slash = false;
    if (!dir) {
        dir = empty;
    } else if (len_dir > 0 && dir[len_dir - 1] != '/') {
        add_slash = true;
    }

    if (!prefix) {
        prefix = empty;
    }

    if (!suffix) {
        suffix = empty;
    }

    // Allocate memory
    char *result = calloc(len_total, sizeof(char));
    if (!result) {
        return NULL;
    }

    const char *fmt = add_slash ? "%s/%s%s" : "%s%s%s";
    int ret = snprintf(result, len_total, fmt, dir, prefix, suffix);
    if (ret < 0 || (size_t)ret >= len_total) {
        free(result);
        return NULL;
    }

    return result;
}

/**
 * \brief Free previously generated names of output files
 * \param[in,out] names Pointer to previously initialized structure
 */
static void
files_mgr_names_free(struct files_mgr_names *names)
{
    free(names->file_lnf);
    free(names->file_index);
    free(names->dir);
}

/**
 * \brief Remove unwanted characters in a path
 *
 * This function removes multiple slashes in the path. The size of the path
 * can be only reduced, so all operations are performed in-situ.
 * \param[in,out] path Path
 */
void
files_mgr_names_sanitize(char *path)
{
    if (!path || path[0] == '\0') {
        // Empty string
        return;
    }

    size_t idx_read = 1;
    size_t idx_write = 1;

    while (path[idx_read] != '\0') {
        if (path[idx_read] == '/' && path[idx_read - 1] == '/') {
            // Skip the slash
            idx_read++;
            continue;
        }

        if (idx_read != idx_write) {
            path[idx_write] = path[idx_read];
        }

        idx_read++;
        idx_write++;
    }

    path[idx_write] = '\0';
}

/**
 * \brief Generate names of output files (for a new window)
 *
 * Based on an internal template and current timestamp generate names of
 * filenames and directories for the output files.
 *
 * \param[in]  mgr   Pointer to a file manager
 * \param[in]  ts    Timestamp of the window
 * \param[out] names Pointer to uninitialized structure
 * \return On success returns 0 and fill \p names. Otherwise returns a non-zero
 *   value and \p names is unchanged.
 */
static int
files_mgr_names_create(const files_mgr_t *mgr, const time_t *ts, struct files_mgr_names *names)
{
    struct files_mgr_names res = {NULL, NULL, NULL};

    // Convert the timestamp to UTC
    struct tm gm_timestamp;
    if (gmtime_r(ts, &gm_timestamp) == NULL) {
        IPX_CTX_ERROR(mgr->ctx, "Files manager error (failed to convert time to UTC).", '\0');
        return 1;
    }

    // Create a subdir path
    const char *subdir_fmt = "/%Y/%m/%d/";
    const size_t subdir_len = 16;
    char subdir[subdir_len];

    if (strftime(subdir, subdir_len, subdir_fmt, &gm_timestamp) == 0) {
        IPX_CTX_ERROR(mgr->ctx, "Files manager error (failed to generate a name of "
            "a subdirectory).", '\0');
        return 1;
    }

    // Generate full name of the directory (+2 = '/' + '\0');
    res.dir = (char *) malloc(PATH_MAX * sizeof(char));
    if (!res.dir) {
        IPX_CTX_ERROR(mgr->ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        return 1;
    }

    const char *main_dir = mgr->paths_tmplt->dir;
    int ret = snprintf(res.dir, PATH_MAX, "%s/%s", main_dir, subdir);
    if (ret < 0 || ret >= PATH_MAX) {
        IPX_CTX_ERROR(mgr->ctx, "File manager error (name of an output directory is probably too "
            "long).", '\0');
        files_mgr_names_free(&res);
        return 1;
    }

    // Create a suffix part of the files
    char *file_suffix = (char *) malloc(PATH_MAX * sizeof(char));
    if (!file_suffix) {
        IPX_CTX_ERROR(mgr->ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        files_mgr_names_free(&res);
        return 1;
    }

    const char *suffix_fmt = mgr->paths_tmplt->suffix_mask;
    if (strftime(file_suffix, PATH_MAX, suffix_fmt, &gm_timestamp) == 0) {
        IPX_CTX_ERROR(mgr->ctx, "Files manager error (failed to generate a name of a storage "
            "file).", '\0');
        files_mgr_names_free(&res);
        free(file_suffix);
        return 1;
    }

    // Create filenames
    if (mgr->mode & FILES_M_LNF) {
        const char *prefix = mgr->paths_tmplt->prefixes.lnf;
        res.file_lnf = files_mgr_names_create_aux(res.dir, prefix, file_suffix);
        if (!res.file_lnf) {
            IPX_CTX_ERROR(mgr->ctx, "Files manager error (an output filename is too probably "
                "too long)", '\0');
            files_mgr_names_free(&res);
            free(file_suffix);
            return 1;
        }
    }

    if (mgr->mode & FILES_M_INDEX) {
        const char *prefix = mgr->paths_tmplt->prefixes.index;
        res.file_index = files_mgr_names_create_aux(res.dir, prefix,
            file_suffix);
        if (!res.file_index) {
            IPX_CTX_ERROR(mgr->ctx, "Files manager error (an output filename is too probably "
                "too long)", '\0');
            files_mgr_names_free(&res);
            free(file_suffix);
            return 1;
        }
    }

    // Sanitize strings
    files_mgr_names_sanitize(res.dir);
    files_mgr_names_sanitize(res.file_index);
    files_mgr_names_sanitize(res.file_lnf);

    // Copy pointers
    free(file_suffix);
    *names = res;
    return 0;
}

int
files_mgr_new_window(files_mgr_t *mgr, const time_t *ts)
{
    IPX_CTX_DEBUG(mgr->ctx, "Files manager - create a new window.", '\0');
    int ret_code = 0;

    // Close/flush LNF file
    if (mgr->outputs.file_lnf) {
        lnf_close(mgr->outputs.file_lnf);
        mgr->outputs.file_lnf = NULL;
    }

    if (mgr->mode & FILES_M_INDEX) {
        if (idx_mgr_save_index(mgr->outputs.index_mgr) != 0) {
            IPX_CTX_WARNING(mgr->ctx, "Files manager error (failed to save current index - "
                "last window won't be indexed).", '\0');
        }
    }

    // Create new file names
    struct files_mgr_names names;
    if (files_mgr_names_create(mgr, ts, &names) != 0) {
        // Invalidate an index window -> do not allow to add new records...
        if (mgr->mode & FILES_M_INDEX) {
            idx_mgr_invalidate(mgr->outputs.index_mgr);
        }
        return 1;
    }

    // Create the directory
    if (utils_mkdir(names.dir) != IPX_OK) {
        const char *err_str;
        ipx_strerror(errno, err_str);
        IPX_CTX_ERROR(mgr->ctx, "Files manager error (failed to create the directory '%s' - %s).",
            names.dir, err_str);

        // Invalidate an index window -> do not allow to add new records...
        if (mgr->mode & FILES_M_INDEX) {
            idx_mgr_invalidate(mgr->outputs.index_mgr);
        }

        files_mgr_names_free(&names);
        return 1;
    }

    // Create LNF file
    if (mgr->mode & FILES_M_LNF) {
        unsigned int flags = LNF_WRITE;
        if (mgr->lnf_params.compress) {
            flags |= LNF_COMP;
        }

        if (lnf_open(&mgr->outputs.file_lnf, names.file_lnf, flags,
                mgr->lnf_params.ident) != LNF_OK) {
            IPX_CTX_WARNING(mgr->ctx, "Files manager error (failed to create the file '%s' - some "
                "records will not be stored).", names.file_lnf);
            ret_code = 1;
        } else {
            IPX_CTX_DEBUG(mgr->ctx, "File manager - the new LNF file '%s'", names.file_lnf);
        }
    }

    // Create Index file
    if (mgr->mode & FILES_M_INDEX) {
        if (idx_mgr_window_new(mgr->outputs.index_mgr, names.file_index)) {
            IPX_CTX_WARNING(mgr->ctx, "Files manager error (failed to create a new window of "
                "Bloom Filter Index).", '\0');
            ret_code = 1;
            idx_mgr_invalidate(mgr->outputs.index_mgr);
        } else {
            IPX_CTX_DEBUG(mgr->ctx, "File manager - the new BF index file '%s'", names.file_index);
        }
    }

    files_mgr_names_free(&names);
    return ret_code;
}

void
files_mgr_invalidate(files_mgr_t *mgr)
{
    // Close/flush LNF file
    if (mgr->outputs.file_lnf) {
        lnf_close(mgr->outputs.file_lnf);
        mgr->outputs.file_lnf = NULL;
    }

    if (mgr->mode & FILES_M_INDEX) {
        if (idx_mgr_save_index(mgr->outputs.index_mgr) != 0) {
            IPX_CTX_WARNING(mgr->ctx, "Files manager error (failed to save current index - last "
                "window wont be indexed).", '\0');
        }
        idx_mgr_invalidate(mgr->outputs.index_mgr);
    }
}

/**
 * \brief Add a LNF record to a LNF file
 * \param[in,out] mgr     Pointer to a file manager
 * \param[in]     rec_ptr Pointer to the LNF record
 * \warning The LNF file must exist.
 * \return On success return 0. Otherwise returns a non-zero value.
 */
static int
files_mgr_add2lnf(files_mgr_t *mgr, void *rec_ptr)
{
    lnf_file_t *file_ptr = mgr->outputs.file_lnf;
    if (!file_ptr) {
        return 1;
    }

    return (lnf_write(file_ptr, rec_ptr) != LNF_OK) ? 1 : 0;
}

/**
 * \brief Add source and destination IP address to a Bloom filter index
 * \note If any of the addresses is missing in a LNF record, the missing address
 *   is just skipped without error.
 * \param[in,out] mgr      Pointer to a file manager
 * \param[in]     rec_ptr  Pointer to the LNF record
 * \warning An internal index (idx) manager must exist.
 * \return On success returns 0. Otherwise returns a non-zero value.
 */
static int
files_mgr_add2idx(files_mgr_t *mgr, void *rec_ptr)
{
    int status = 0;
    idx_mgr_t *idx_ptr = mgr->outputs.index_mgr;

    const size_t len = 16; // For IPv4 & IPv6
    unsigned char buffer[len];

    // Add source address
    memset(buffer, 0, len);
    if (lnf_rec_fget(rec_ptr, LNF_FLD_SRCADDR, buffer) != LNF_OK) {
        // It is possible that a record do not have an SRC IP address
        IPX_CTX_DEBUG(mgr->ctx, "Unable to get a SRC IP address and insert it into a Bloom "
            "filter Index.", '\0');
    } else {
        // Cannot be null, only an internal window can be broken
        status |= idx_mgr_add(idx_ptr, buffer, len);
    }

    // Add destination address
    memset(buffer, 0, len);
    if (lnf_rec_fget(rec_ptr, LNF_FLD_DSTADDR, buffer) != LNF_OK) {
        // It is possible that a record do not have an DST IP address
        IPX_CTX_DEBUG(mgr->ctx, "Unable to get a DST IP address and insert it into a Bloom "
            "Filter Index.", '\0');
    } else {
        // Cannot be null, only an internal window can be broken
        status |= idx_mgr_add(idx_ptr, buffer, len);
    }

    return status;
}

int
files_mgr_add_record(files_mgr_t *mgr, lnf_rec_t *rec_ptr)
{
    int status = 0;

    // LNF file
    if (mgr->mode & FILES_M_LNF) {
        status |= files_mgr_add2lnf(mgr, rec_ptr);
    }

    // Index file
    if (mgr->mode & FILES_M_INDEX) {
        status |= files_mgr_add2idx(mgr, rec_ptr);
    }

    return status;
}

const char *
files_mgr_get_storage_dir(files_mgr_t *mgr)
{
    return mgr->paths_tmplt->dir;
}
