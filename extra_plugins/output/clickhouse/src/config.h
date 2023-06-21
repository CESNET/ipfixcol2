/**
 * \file
 * \author Petr Szczurek <xszczu00@stud.fit.vut.cz>
 * \brief Parser of an XML configuration (header file)
 *
 * Copyright(c) 2023 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <ipfixcol2.h>
#include "stdint.h"

/** Configuration of a instance of the plugin */
struct config {
    // TODO
    int dummy;
};

/**
 * \brief Create and parse configuration of the plugin
 * \param[in] ctx    Instance context
 * \param[in] params XML parameters
 * \return Pointer to the parse configuration of the instance on success
 * \return NULL if arguments are not valid or if a memory allocation error has occurred
 */
struct config *
config_create(ipx_ctx_t *ctx, const char *params);

/**
 * \brief Destroy parsed configuration
 * \param[in] cfg Parsed configuration
 */
void
config_destroy(struct config *cfg);

#endif // CONFIG_H
