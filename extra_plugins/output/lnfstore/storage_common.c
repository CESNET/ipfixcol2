/**
 * \file storage_common.c
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Common function for storage managers (source file)
 *
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
#include <sys/stat.h>
#include "storage_common.h"
#include "translator.h"

files_mgr_t *
stg_common_files_mgr_create(ipx_ctx_t *ctx, const struct conf_params *params, const char *dir)
{
    struct files_mgr_idx_param *param_idx_ptr = NULL;
    enum FILES_MODE mode = FILES_M_LNF; // We always want to create LNF files

    // Define an output directory and filenames
    struct files_mgr_paths paths;
    memset(&paths, 0, sizeof(paths));

    paths.dir = dir;
    paths.suffix_mask = params->files.suffix;

    paths.prefixes.lnf = params->file_lnf.prefix;
    if (params->file_index.en) {
        paths.prefixes.index = params->file_index.prefix;
    }

    // Define LNF parameters
    struct files_mgr_lnf_param param_lnf;
    memset(&param_lnf, 0, sizeof(param_lnf));

    param_lnf.compress = params->file_lnf.compress;
    param_lnf.ident = params->file_lnf.ident;

    // Define Index parameters
    struct files_mgr_idx_param param_idx;
    memset(&param_idx, 0, sizeof(param_idx));

    if (params->file_index.en) {
        param_idx.autosize = params->file_index.autosize;
        param_idx.item_cnt = params->file_index.est_cnt;
        param_idx.prob = params->file_index.fp_prob;

        param_idx_ptr = &param_idx;
        mode |= FILES_M_INDEX;
    }

    return files_mgr_create(ctx, mode, &paths, &param_lnf, param_idx_ptr);
}

int
stg_common_dir_exists(const char *dir)
{
    struct stat sb;
    if (stat(dir, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        // Exists
        return 0;
    } else {
        // Doesn't exist or search permission is denied
        return 1;
    }
}
