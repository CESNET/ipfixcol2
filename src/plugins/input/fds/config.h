/**
 * \file src/plugins/input/fds/config.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Configuration parser of FDS input plugin (source file)
 * \date 2020
 */

/* Copyright (C) 2020 CESNET, z.s.p.o.
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

#ifndef FDS_CONFIG_H
#define FDS_CONFIG_H

#include <ipfixcol2.h>
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Configuration of a instance of the IPFIX plugin                                              */
struct fds_config {
    /** File pattern                                                                             */
    char *path;
    /** Size of IPFIX Messages to generate                                                       */
    uint16_t msize;
    /** Enable asynchronous I/O                                                                  */
    bool async;
};

/**
 * @brief Parse configuration of the plugin
 * @param[in] ctx    Instance context
 * @param[in] params XML parameters
 * @return Pointer to the parse configuration of the instance on success
 * @return NULL if arguments are not valid or if a memory allocation error has occurred
 */
struct fds_config *
config_parse(ipx_ctx_t *ctx, const char *params);

/**
 * @brief Destroy parsed configuration
 * @param[in] cfg Parsed configuration
 */
void
config_destroy(struct fds_config *cfg);

#ifdef __cplusplus
}
#endif

#endif // FDS_CONFIG_H
