/**
 * @file   include/ipfixcol2/utils.h
 * @author Lukas Hutak <lukas.hutak@cesnet.cz>
 * @brief  Auxiliary utilities for plugins (header file)
 * @date   June 2019
 *
 * Copyright(c) 2019 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IPX_UTILS_H
#define IPX_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ipfixcol2/api.h>
#include <sys/stat.h> // mkdir file permissions

/**
* \defgroup ipxSource Transport session identification
* \ingroup publicAPIs
* \brief Transport session interface
*
* Data types and API functions for identification and management of Transport
* session identification. The Exporting Process uses the Transport Session to
* send messages from multiple _independent_ Observation Domains to the
* Collecting Process. Moreover, in case of SCTP session messages are also send
* over _independent_ streams.
*
* Following structures represents Transport session between Exporting process
* and Collecting Process. However, proper processing of flows also requires
* distinguishing Observation Domain IDs and Stream identifications out of
* scope of these structures.
*
* @{
*/

/**
 * @brief Default file permission of newly created directories
 * @note Read/write/execute for a user and his group, read/execute for others.
 */
#define IPX_UTILS_MKDIR_DEF (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)

/**
 * @brief Create recursively a directory
 *
 * @note
 *   File permission @p mode only affects newly created directories. In other
 *   words, if a directory (or subdirectory) already exists, file permission
 *   bits @p mode are not applied.
 * @note
 *   The function is implemented as "recursive" wrapper over standard mkdir
 *   function. See man 3 mkdir for more information.
 * @param[in] path Full directory path to create
 * @param[in] mode The file permission bits of the new directories
 *   (see default value #IPX_UTILS_MKDIR_DEF)
 * @return #IPX_OK on success
 * @return #IPX_ERR_DENIED otherwise and errno is set appropriately.
 */
IPX_API int
ipx_utils_mkdir(const char *path, mode_t mode);

/**@}*/

#ifdef __cplusplus
}
#endif
#endif // IPX_UTILS_H
