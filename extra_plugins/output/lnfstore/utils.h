/**
 * \file utils.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Simple utils (header file)
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
* This software is provided ``as is, and any express or implied
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
*/

#ifndef LNFSTORE_UTILS_H
#define LNFSTORE_UTILS_H

#include <ipfixcol2.h>

/**
 * \brief Formats the path string according to the format specification
 * \param[in] original Path with special character sequences
 * \param[in] ctx      Instance context (just for log!)
 * \return On success returns a pointer to the newly created path.
 * \return Otherwise returns the NULL pointer and errno is set appropriately..
 */
char *
utils_path_preprocessor(const char *original);

/**
 * \brief Create recursively a directory
 * \param[in] path Full directory path
 * \return On success returns #IPX_OK.
 * \return Otherwise returns a #IPX_ERR_DENIED value and errno is set appropriately.
 */
int
utils_mkdir(const char *path);

#endif //LNFSTORE_UTILS_H
