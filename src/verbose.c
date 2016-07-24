/**
 * \file src/verbose.c
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Implementation of functions for printing status messages
 */

/* Copyright (C) 2016 CESNET, z.s.p.o.
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

#include <ipfixcol2/verbose.h>
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>

// Default is to print errors only
static enum IPX_VERBOSITY_LEVEL verbose = IPX_VERBOSITY_ERROR;
// Do not use syslog unless specified otherwise
static int use_syslog = 0;

// Get verbosity level of the collector
enum IPX_VERBOSITY_LEVEL ipx_verbosity_get_level()
{
	return verbose;
}

// Set verbosity level of the collector
void ipx_verbosity_set_level(enum IPX_VERBOSITY_LEVEL new_level)
{
	if (new_level < IPX_VERBOSITY_ERROR || new_level > IPX_VERBOSITY_DEBUG) {
		new_level = IPX_VERBOSITY_ERROR;
	}
	
	verbose = new_level;
}

// Enable reporting to the system log (syslog)
void ipx_verbosity_syslog_enable()
{
	if (use_syslog != 0) {
		// Already enabled
		return;
	}
	
	// TODO: name from the configuration
	openlog("IPFIXcol2", LOG_PID, LOG_DAEMON);
	use_syslog = 1;
}

// Disable reporting to the system log (syslog)
void ipx_verbosity_syslog_disable()
{
	if (use_syslog == 0) {
		// Already disabled
		return;
	}
	
	closelog();
	use_syslog = 0;
}

// Common printing function
void ipx_verbosity_print(enum IPX_VERBOSITY_LEVEL level, const char *format,...)
{
	va_list ap;
	int priority;

	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);

	if (use_syslog) {
		va_start(ap, format);
		switch (level) {
		case IPX_VERBOSITY_ERROR:
			priority = LOG_ERR;
			break;
		case IPX_VERBOSITY_WARNING:
			priority = LOG_WARNING;
			break;
		case IPX_VERBOSITY_DEBUG:
			priority = LOG_DEBUG;
			break;
		default:
			priority = LOG_INFO;
			break;
		}

		vsyslog(priority, format, ap);
		va_end(ap);
	}
}

