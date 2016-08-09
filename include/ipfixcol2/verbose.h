/**
 * \file include/ipfixcol2/verbose.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Functions for printing status messages (public API)
 * \date 2016
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

#ifndef VERBOSE_H_
#define VERBOSE_H_

#include <ipfixcol2/api.h>

/**
 * \defgroup ipxVerboseAPI Status messages
 * \ingroup publicAPIs
 * \brief Public functions for printing status messages
 *
 * By default, verbosity-level is set to report only errors
 * i.e. #IPX_VERBOSITY_ERROR and reporting to the system logger (syslog) is
 * disabled.
 *
 * @{
 */

/**
 * \brief Verbosity level of a message
 *
 * For detailed description of each verbosity level see #MSG_ERROR,
 * #MSG_WARNING, #MSG_INFO and #MSG_DEBUG
 */
enum IPX_VERBOSITY_LEVEL {
	IPX_VERBOSITY_ERROR = 0,   /**< Error message (default)        */
	IPX_VERBOSITY_WARNING,     /**< Warning message                */
	IPX_VERBOSITY_INFO,        /**< Informational message          */
	IPX_VERBOSITY_DEBUG        /**< Debug message                  */
};

/**
 * \brief Macro for printing error message
 *
 * Use this when something went really wrong, e.g., memory errors or disk full.
 * \param[in] module Identification of program component that generated this
 *   message
 * \param[in] format Format string (see manual page for "printf" family)
 * \param[in] ...    Variable number of arguments for the format string
 */
#define MSG_ERROR(module, format, ...) \
	if (ipx_verbosity_get_level() >= IPX_VERBOSITY_ERROR) { \
		ipx_verbosity_print(IPX_VERBOSITY_ERROR, "ERROR: %s: " format "\n", \
			module, ## __VA_ARGS__); \
	}

/**
 * \brief Macro for printing warning message
 *
 * Use this when something is not right, but an action can continue.
 * \param[in] module Identification of program component that generated this
 *   message
 * \param[in] format Format string (see manual page for "printf" family)
 * \param[in] ...    Variable number of arguments for the format string
 */
#define MSG_WARNING(module, format, ...) \
	if (ipx_verbosity_get_level() >= IPX_VERBOSITY_WARNING) { \
		ipx_verbosity_print(IPX_VERBOSITY_WARNING, "WARNING: %s: " format "\n", \
			module, ## __VA_ARGS__); \
	}

/**
 * \brief Macro for printing informational message
 *
 * Use this when you have something to say, but you don't expect anyone to care.
 * \param[in] module Identification of program component that generated this
 *   message
 * \param[in] format Format string (see manual page for "printf" family)
 * \param[in] ...    Variable number of arguments for the format string
 */
#define MSG_INFO(module, format, ...) \
	if (ipx_verbosity_get_level() >= IPX_VERBOSITY_INFO) { \
		ipx_verbosity_print(IPX_VERBOSITY_INFO, "INFO: %s: " format "\n", \
			module, ## __VA_ARGS__); \
	}

/**
 * \brief Macro for printing debug message
 *
 *
 * \param[in] module Identification of program component that generated this
 *   message
 * \param[in] format Format string (see manual page for "printf" family)
 * \param[in] ...    Variable number of arguments for the format string
 */
#define MSG_DEBUG(module, format, ...) \
	if (ipx_verbosity_get_level() >= IPX_VERBOSITY_DEBUG) { \
		ipx_verbosity_print(IPX_VERBOSITY_DEBUG, "DEBUG: %s: " format "\n", \
			module, ## __VA_ARGS__); \
	}

/**
 * \brief Macro for printing common messages, without severity prefix.
 *
 * In syslog, all of these messages will have LOG_INFO severity.
 * \param[in] level  The verbosity level at which this message should be printed
 * \param[in] format Format string (see manual page for "printf" family)
 * \param[in] ...    Variable number of arguments for the format string
 */
#define MSG_COMMON(level, format, ...) \
	if (ipx_verbosity_get_level() >= level) { \
		ipx_verbosity_print(IPX_VERBOSITY_INFO, format"\n", ## __VA_ARGS__); \
	}

/**
 * \brief Get verbosity level of the collector
 * \return Current verbosity level
 */
API enum IPX_VERBOSITY_LEVEL ipx_verbosity_get_level();

/**
 * \brief Set verbosity level of the collector
 *
 * When invalid value is used as \p new_level, the verbosity level will stay
 * unchaned.
 * \param[in] new_level New verbosity-level
 */
API void ipx_verbosity_set_level(enum IPX_VERBOSITY_LEVEL new_level);

/**
 * \brief Enable reporting to the system log (syslog)
 * \remark By default, reporting is disabled.
 */
API void ipx_verbosity_syslog_enable();

/**
 * \brief Disable reporting to the system log (syslog)
 */
API void ipx_verbosity_syslog_disable();

/**
 * \brief Common printing function
 *
 * \param[in] level  Verbosity level of the message (for syslog severity)
 * \param[in] format Format string (see manual page for "printf" family)
 * \param[in] ...    Variable number of arguments for the format string
 */
API void ipx_verbosity_print(enum IPX_VERBOSITY_LEVEL level,
		const char *format, ...);

/**@}*/
#endif /* VERBOSE_H_ */
