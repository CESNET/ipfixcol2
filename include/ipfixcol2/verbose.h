/**
 * \file include/ipfixcol2/verbose.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Functions for printing status messages (public API)
 * \date 2016
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

#ifndef IPX_VERBOSE_H
#define IPX_VERBOSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <string.h>
#include <ipfixcol2/api.h>

/** Forward declaration of a plugin context */
typedef struct ipx_ctx ipx_ctx_t;

/**
 * \defgroup ipxVerboseAPI Status messages
 * \ingroup publicAPIs
 * \brief Public functions for printing status messages
 *
 * By default, verbosity-level is set to report only errors
 * i.e. #IPX_VERB_ERROR and reporting to the system logger (syslog) is disabled.
 *
 * @{
 */

/**
 * \brief Verbosity level of a message
 *
 * For detailed description of each verbosity level see #IPX_ERROR, #IPX_WARNING, #IPX_INFO and
 * #IPX_DEBUG.
 */
enum ipx_verb_level {
    IPX_VERB_NONE  = 0,   /**< Ignore all messages            */
    IPX_VERB_ERROR,       /**< Error message (default)        */
    IPX_VERB_WARNING,     /**< Warning message                */
    IPX_VERB_INFO,        /**< Informational message          */
    IPX_VERB_DEBUG        /**< Debug message                  */
};

/**
 * \def IPX_CTX_ERROR
 * \brief Macro for printing an error message
 *
 * Use this when something went really wrong, e.g., memory errors or disk full.
 * \param[in] ctx Identification of a plugin that generates this message
 * \param[in] fmt Format string (see manual page for "printf" family)
 * \param[in] ...   Variable number of arguments for the format string
 */
#define IPX_CTX_ERROR(ctx, fmt, ...)                                        \
    if (ipx_ctx_verb_get(ctx) >= IPX_VERB_ERROR) {                          \
        ipx_verb_ctx_print(IPX_VERB_ERROR, (ctx), (fmt), ## __VA_ARGS__);   \
    }

/**
 * \def IPX_CTX_WARNING
 * \brief Macro for printing a warning message
 *
 * Use this when something is not right, but an action can continue.
 * \param[in] ctx Identification of a plugin that generates this message.
 * \param[in] fmt Format string (see manual page for "printf" family)
 * \param[in] ... Variable number of arguments for the format string
 */
#define IPX_CTX_WARNING(ctx, fmt, ...)                                      \
    if (ipx_ctx_verb_get(ctx) >= IPX_VERB_WARNING) {                        \
        ipx_verb_ctx_print(IPX_VERB_WARNING, (ctx), (fmt), ## __VA_ARGS__); \
    }

/**
 * \def IPX_CTX_INFO
 * \brief Macro for printing an informational message
 *
 * Use this when you have something to say, but you don't expect anyone to care.
 * \param[in] ctx Identification of a plugin that generates this message
 * \param[in] fmt Format string (see manual page for "printf" family)
 * \param[in] ... Variable number of arguments for the format string
 */
#define IPX_CTX_INFO(ctx, fmt, ...)                                         \
    if (ipx_ctx_verb_get(ctx) >= IPX_VERB_INFO) {                           \
        ipx_verb_ctx_print(IPX_VERB_INFO, (ctx), (fmt), ## __VA_ARGS__);    \
    }

/**
 * \def IPX_CTX_DEBUG
 * \brief Macro for printing a debug message
 *
 * All information that is only interesting for developers.
 * \param[in] ctx Identification of a plugin that generates this message.
 * \param[in] fmt Format string (see manual page for "printf" family)
 * \param[in] ... Variable number of arguments for the format string
 */
#define IPX_CTX_DEBUG(ctx, fmt, ...)                                        \
    if (ipx_ctx_verb_get(ctx) >= IPX_VERB_DEBUG) {                          \
        ipx_verb_ctx_print(IPX_VERB_DEBUG, (ctx), (fmt), ## __VA_ARGS__);   \
    }

/**
 * \brief Common printing function
 *
 * Never use this function directly. Always use auxiliary macros.
 * \param[in] level  Verbosity level of the message (for syslog severity)
 * \param[in] ctx    Plugin context
 * \param[in] fmt    Format string (see manual page for "printf" family)
 * \param[in] ...    Variable number of arguments for the format string
 */
IPX_API void
ipx_verb_ctx_print(enum ipx_verb_level level, const ipx_ctx_t *ctx, const char *fmt, ...);

/** Size of an error buffer message                                               */
#define IPX_STRERROR_SIZE 128

/**
* \brief Convert standard error code to an error string (internal function)
* \param[in] errnum      Error code
* \param[in] buffer      Buffer for the error string
* \param[in] buffer_size Size of the buffer
* \return #IPX_OK on success and the \p buffer is properly filled
* \return #IPX_ERR_ARG in case of invalid error code and the \p buffer is filled with an error
 *   message
*/
IPX_API int
ipx_strerror_fn(int errnum, char *buffer, size_t buffer_size);

/**
 * \def ipx_strerror
 * \brief Re-entrant strerror wrapper
 *
 * Main purpose is to solve issues with different strerror_r definitions.
 * \warning The function is defined as a macro that creates a local buffer for an error message.
 *   Therefore, it cannot be called multiple times at the same scope.
 * \code{.c}
 *   const char *err_str;
 *   ipx_strerror(errno, err_str);
 *   printf("ERROR: %s\n", err_str);
 * \endcode
 * \param[in]  errnum An error code
 * \param[out] buffer Pointer to the local buffer
 */
#define ipx_strerror(errnum, buffer)                                                       \
    char ipx_strerror_buffer[IPX_STRERROR_SIZE];                                           \
    ipx_strerror_fn((errnum), ipx_strerror_buffer, IPX_STRERROR_SIZE);                     \
    (buffer) = ipx_strerror_buffer;

/**@}*/
#ifdef __cplusplus
}
#endif
#endif // IPX_VERBOSE_H
