/**
 * \file src/core/fpipe.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Feedback pipe(internal header file)
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

#ifndef IPFIXCOL_FPIPE_H
#define IPFIXCOL_FPIPE_H

#include <ipfixcol2.h>

/** Internal data type for feedback type */
typedef struct ipx_fpipe ipx_fpipe_t;

/**
 * \brief Create a feedback pipe
 * \return Pointer to the pipe or NULL
 */
IPX_API ipx_fpipe_t *
ipx_fpipe_create();

/**
 * \brief Destroy a feedback pipe
 * \param[in] fpipe Pipe to destroy
 */
IPX_API void
ipx_fpipe_destroy(ipx_fpipe_t *fpipe);

/**
 * \brief Send a request to close a Transport Session
 *
 * \param[in] fpipe   Feedback pipe
 * \param[in] ts Transport Session to close
 * \return #IPX_OK on success
 * \return #IPX_ERR_ARG if a fatal internal error has occurred
 */
IPX_API int
ipx_fpipe_write(ipx_fpipe_t *fpipe, const struct ipx_session *ts);

/**
 * \brief Get a request to close a Transport Session
 *
 * The function tries is non-blocking.
 * \warning Do not try to access data in the pointer! It could be already freed! Always use
 *   only value of the pointer.
 * \param[in]  fpipe Feedback pipe
 * \param[out] ts    Transport Session to close
 * \return #IPX_OK if the session is prepared
 * \return #IPX_ERR_NOTFOUND if no session is available
 * \return #IPX_ERR_ARG if a fatal internal error has occurred
 */
IPX_API int
ipx_fpipe_read(ipx_fpipe_t *fpipe, const struct ipx_session **ts);

#endif // IPFIXCOL_FPIPE_H
