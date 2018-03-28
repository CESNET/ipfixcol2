/**
 * \file src/core/verbose.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Functions for printing status messages (source file)
 * \date 2016-2018
 */

/* Copyright (C) 2016-2018 CESNET, z.s.p.o.
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

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <stdbool.h>

#include <ipfixcol2.h>
#include "build_config.h"
#include "context.h"  // Internal header
#include "verbose.h"


/** Default verbosity level                      */
static enum ipx_verb_level vlevel = IPX_VERB_ERROR;
/** Do not use syslog unless specified otherwise */
static bool use_syslog = false;

// Get verbosity level of the collector
enum ipx_verb_level
ipx_verb_level_get()
{
    return vlevel;
}

// Set verbosity level of the collector
void
ipx_verb_level_set(enum ipx_verb_level level)
{
    if (level < IPX_VERB_NONE || level > IPX_VERB_DEBUG) {
        return;
    }

    vlevel = level;
}

// Enable/disable reporting to the system log (syslog)
void
ipx_verb_syslog(bool enable)
{
    if ((enable && use_syslog) || (!enable && !use_syslog)) {
        // Nothing to do...
        return;
    }

    if (enable) {
        // Enable
        openlog(IPX_BUILD_APP_NAME, LOG_PID, LOG_DAEMON);
    } else {
        // Disable
        closelog();
    }
    use_syslog = enable;
}

/**
 * \brief Convert internal message type to syslog type
 * \param level IPFIXcol message type
 * \return Syslog type
 */
static inline int
ipx_verb_level2syslog(const enum ipx_verb_level level)
{
    switch (level) {
    case IPX_VERB_ERROR:   return LOG_ERR;
    case IPX_VERB_WARNING: return LOG_WARNING;
    case IPX_VERB_INFO:    return LOG_INFO;
    case IPX_VERB_DEBUG:   return LOG_DEBUG;
    default:               break;
    }

    // Unhandled type
    return LOG_ERR;
}

void
ipx_verb_ctx_print(enum ipx_verb_level level, const ipx_ctx_t *ctx, const char *fmt, ...)
{
    static const char *err_inter = "<internal error - failed to format a message>\n";
    static const char *fmt_pattern[] = {
        [IPX_VERB_ERROR]   = "ERROR: %s: %s\n",
        [IPX_VERB_WARNING] = "WARNING: %s: %s\n",
        [IPX_VERB_INFO]    = "INFO: %s: %s\n",
        [IPX_VERB_DEBUG]   = "DEBUG: %s: %s\n"
    };

    const char *plugin = ipx_ctx_name_get(ctx);
    const size_t fmt_size = 512;
    char fmt_buffer[fmt_size];

    // Create a new format message
    int rv = snprintf(fmt_buffer, fmt_size, fmt_pattern[level], plugin, fmt);
    if (rv < 0 || ((size_t) rv) >= fmt_size) {
        // Error
        printf(fmt_pattern[level], plugin, err_inter);
    } else {
        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt_buffer, ap);
        va_end(ap);
    }

    if (use_syslog) {
        int prio = ipx_verb_level2syslog(level);
        if (rv < 0 || ((size_t) rv) >= fmt_size) {
            // Error
            syslog(prio, fmt_pattern[level], plugin, err_inter);
        } else {
            va_list ap;
            va_start(ap, fmt);
            vsyslog(prio, fmt_buffer, ap);
            va_end(ap);
        }
    }
}

void
ipx_verb_print(enum ipx_verb_level level, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    if (use_syslog) {
        int prio = ipx_verb_level2syslog(level);
        va_start(ap, fmt);
        vsyslog(prio, fmt, ap);
        va_end(ap);
    }
}
