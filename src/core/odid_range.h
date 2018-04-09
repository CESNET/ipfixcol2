/**
 * \file src/core/odid_range.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief ODID range filter (internal header file)
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

#ifndef IPFIXCOL_ODID_RANGE_H
#define IPFIXCOL_ODID_RANGE_H

#include <stdbool.h>
#include <ipfixcol2/api.h>

/** Internal data type                                                      */
typedef struct ipx_orange ipx_orange_t;

/** Output ODID filter type                                                 */
enum ipx_odid_filter_type {
    /** Filter not defined (process all ODIDs)                              */
    IPX_ODID_FILTER_NONE,
    /** Process only ODIDs that match a filter                              */
    IPX_ODID_FILTER_ONLY,
    /** Process only ODIDs that do NOT match a filter                       */
    IPX_ODID_FILTER_EXCEPT
};

/**
 * \brief Create a new ODID range filter
 * \return Pointer or NULL (memory allocation error)
 */
IPX_API ipx_orange_t *
ipx_orange_create();

/**
 * \brief Destroy an ODID range filter
 * \param[in] range Filter to destroy
 */
IPX_API void
ipx_orange_destroy(ipx_orange_t *range);

/**
 * \brief Parse a filter expression
 *
 * Expected filter expression is represented as comma separated list of unsigned numbers
 * and intervals. Interval is all the numbers between two given numbers separated by a dash.
 * If one number of the interval is missing, the minimum or the maximum is used by default.
 * White spaces in the expression are ignored.
 * For example, "1-5, 7, 10-" represents all ODIDs except 0, 6, 8 and 9
 *
 * \note
 *   In case of error, the state of the filter is undefined and must be destroyed or
 *   an expression must be parsed again.
 *
 * \param[in] range ODID range filter to configure
 * \param[in] expr  Filter expression
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT if the expression is empty or malformed
 * \return #IPX_ERR_NOMEM if a memory allocation error has occurred
 */
IPX_API int
ipx_orange_parse(ipx_orange_t *range, const char *expr);

/**
 * \brief Check if an ODID value is in the range
 * \param[in] range ODID range filter
 * \param[in] odid  Observation Domain ID to test
 * \return True or false
 */
IPX_API bool
ipx_orange_in(const ipx_orange_t *range, uint32_t odid);

/**
 * \brief Dump filter to standard output
 *
 * Show parsed nodes and their values.
 * \param range ODID range filter
 */
IPX_API void
ipx_orange_print(const ipx_orange_t *range);


#endif //IPFIXCOL_ODID_RANGE_H
