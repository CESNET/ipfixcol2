/**
 * \file storage_basic.h
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Basic storage management (source file)
 *
 * Copyright (C) 2015-2017 CESNET, z.s.p.o.
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

#include "lnfstore.h"

#include "storage_basic.h"
#include "storage_common.h"
#include "configuration.h"

/** \brief Basic storage structure */
struct stg_basic_s {
    /** Instance context (only for logs!)   */
    ipx_ctx_t *ctx;
    /** Pointer to the plugin configuration */
    const struct conf_params *params;
    files_mgr_t *mgr; /**< Output files     */
};


stg_basic_t *
stg_basic_create(ipx_ctx_t *ctx, const struct conf_params *params)
{
    // Prepare the internal structure
    stg_basic_t *instance = (stg_basic_t *) calloc(1, sizeof(*instance));
    if (!instance) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        return NULL;
    }

    // Create an output file manager
    files_mgr_t * mgr;
    mgr = stg_common_files_mgr_create(ctx, params, params->files.path);
    if (!mgr) {
        IPX_CTX_ERROR(ctx, "Failed to create output manager.", '\0');
        free(instance);
        return NULL;
    }

    instance->params = params;
    instance->mgr = mgr;
    instance->ctx = ctx;
    return instance;
}

void
stg_basic_destroy(stg_basic_t *storage)
{
    files_mgr_destroy(storage->mgr);
    free(storage);
}

int
stg_basic_store(stg_basic_t *storage, lnf_rec_t *rec)
{
    return files_mgr_add_record(storage->mgr, rec);
}

int
stg_basic_new_window(stg_basic_t *storage, time_t window)
{
    // Check if the output directory already exists
    const char *dir_path = storage->params->files.path;
    if (stg_common_dir_exists(dir_path)) {
        files_mgr_invalidate(storage->mgr);
        IPX_CTX_ERROR(storage->ctx, "Failed to create a new time window. All data will be lost "
            "(output directory '%s' doesn't exists or search permission is denied for one or more "
            "directories in the path).", dir_path);
        return 1;
    }

    int ret = files_mgr_new_window(storage->mgr, &window);
    if (ret) {
        IPX_CTX_WARNING(storage->ctx, "New time window is not properly created.", '\0');
        return 1;
    } else {
        IPX_CTX_INFO(storage->ctx, "New time window successfully created.", '\0');
        return 0;
    }
}



