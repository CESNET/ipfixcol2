/**
 * \file src/core/verbose.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Functions for printing status messages (internal API)
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
 * This software is provided ``as'' is, and any express or implied
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

#ifndef IPX_VERBOSE_INTERNAL_H
#define IPX_VERBOSE_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ipfixcol2/verbose.h>

/**
 * \def IPX_ERROR
 * \brief Internal macro for printing an error message of IPFIXcol components
 *
 * Use this when something went really wrong, e.g., memory errors or disk full.
 * \param[in] module Identification of a plugin that generates this message
 * \param[in] fmt    Format string (see manual page for "printf" family)
 * \param[in] ...    Variable number of arguments for the format string
 */
#define IPX_ERROR(module, fmt, ...)                                                         \
    if (ipx_verb_level_get() >= IPX_VERB_ERROR) {                                           \
        ipx_verb_print(IPX_VERB_ERROR, "ERROR: %s: " fmt "\n", module, ## __VA_ARGS__);     \
    }

/**
 * \def IPX_WARNING
 * \brief Internal macro for printing a warning message of IPFIXcol components
 *
 * Use this when something is not right, but an action can continue.
 * \param[in] module Identification of a plugin that generates this message.
 * \param[in] fmt    Format string (see manual page for "printf" family)
 * \param[in] ...    Variable number of arguments for the format string
 */
#define IPX_WARNING(module, fmt, ...)                                                       \
    if (ipx_verb_level_get() >= IPX_VERB_WARNING) {                                         \
        ipx_verb_print(IPX_VERB_WARNING, "WARNING: %s: " fmt "\n", module, ## __VA_ARGS__); \
    }

/**
 * \def IPX_INFO
 * \brief Internal macro for printing an informational message of IPFIXcol components
 *
 * Use this when you have something to say, but you don't expect anyone to care.
 * \param[in] module Identification of a plugin that generates this message
 * \param[in] fmt    Format string (see manual page for "printf" family)
 * \param[in] ...    Variable number of arguments for the format string
 */
#define IPX_INFO(module, fmt, ...)                                                          \
    if (ipx_verb_level_get() >= IPX_VERB_INFO) {                                            \
        ipx_verb_print(IPX_VERB_INFO, "INFO: %s: " fmt "\n", module, ## __VA_ARGS__);       \
    }

/**
 * \def IPX_DEBUG
 * \brief Internal macro for printing an debug message of IPFIXcol components
 *
 * All information that is only interesting for developers.
 * \param[in] module Identification of a plugin that generates this message.
 * \param[in] fmt    Format string (see manual page for "printf" family)
 * \param[in] ...    Variable number of arguments for the format string
 */
#define IPX_DEBUG(module, fmt, ...)                                                         \
    if (ipx_verb_level_get() >= IPX_VERB_DEBUG) {                                           \
        ipx_verb_print(IPX_VERB_DEBUG, "DEBUG: %s: " fmt "\n", module, ## __VA_ARGS__);     \
    }

/**
 * \brief Enable/disable reporting to the system log (syslog)
 * \remark By default, reporting is disabled.
 */
IPX_API void
ipx_verb_syslog(bool enable);

/**
 * \brief Get default verbosity level of the collector
 * \return Current verbosity level
 */
IPX_API enum ipx_verb_level
ipx_verb_level_get();

/**
 * \brief Set default verbosity level of the collector
 * \param[in] level New verbosity-level
 */
IPX_API void
ipx_verb_level_set(enum ipx_verb_level level);

/**
 * \brief Internal printing function
 *
 * \param[in] level  Verbosity level of the message (for syslog severity)
 * \param[in] fmt    Format string (see manual page for "printf" family)
 * \param[in] ...    Variable number of arguments for the format string
 */
IPX_API void
ipx_verb_print(enum ipx_verb_level level, const char *fmt, ...);

/**@}*/
#ifdef __cplusplus
}
#endif
#endif // IPX_VERBOSE_INTERNAL_H
