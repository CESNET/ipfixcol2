/**
 * \file src/plugins/input/dummy/config.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Example parser of an XML configuration (header file)
 * \date 2018
 */

/* Copyright (C) 2018 CESNET, z.s.p.o.
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

#ifndef CONFIG_H
#define CONFIG_H

#include <ipfixcol2.h>
#include "stdint.h"

/** Configuration of a instance of the dummy plugin      */
struct instance_config {
    /** Observation Domain ID of generated messages      */
    uint32_t odid;
    /** Sleep time                                       */
    struct timespec sleep_time;

    /** Number of messages to generate before failure    */
    unsigned int fail_after;
    /** Type of failure (i.e. return code)               */
    int fail_type;
};

/**
 * \brief Parse configuration of the plugin
 * \param[in] ctx    Instance context
 * \param[in] params XML parameters
 * \return Pointer to the parse configuration of the instance on success
 * \return NULL if arguments are not valid or if a memory allocation error has occurred
 */
struct instance_config *
config_parse(ipx_ctx_t *ctx, const char *params);

/**
 * \brief Destroy parsed configuration
 * \param[in] cfg Parsed configuration
 */
void
config_destroy(struct instance_config *cfg);

#endif // CONFIG_H
