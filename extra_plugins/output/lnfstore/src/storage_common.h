/**
 * \file storage_common.h
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \brief Common function for storage managers (header file)
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

#ifndef STORAGE_COMMON_H
#define STORAGE_COMMON_H

#include "files_manager.h"
#include "configuration.h"


/**
 * \brief Create a file manager for output files
 *
 * Based on parsed parameters from the XML plugin configuration defines
 * names of the output files (LNF and Bloom filter index) and creates
 * the new file manager.
 *
 * \note The \p dir parameter is not taken automatically from the plugin
 *   configuration because in profile mode each channel uses unique output
 *   directory.
 *
 * \param[in] ctx    Instance context (only for logs!)
 * \param[in] params Plugin configuration
 * \param[in] dir    Output directory
 * \return On success returns a pointer to the manager. Otherwise returns
 *   NULL.
 */
files_mgr_t *
stg_common_files_mgr_create(ipx_ctx_t *ctx, const struct conf_params *params, const char *dir);

/**
 * \brief Check if a directory exists
 * \param[in] dir Directory path
 * \return If the directory exists returns 0. Otherwise (the directory doesn't
 *   exist or search permission is denied) returns a non-zero value.
 */
int
stg_common_dir_exists(const char *dir);

#endif // STORAGE_COMMON_H
