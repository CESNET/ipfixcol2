/**
 * \file src/core/utils.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Internal core utilities (source file)
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

// Make sure that XSI-version is used
#undef _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L


#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <ipfixcol2/api.h>
#include <ipfixcol2/verbose.h>
#ifdef __cplusplus
}
#endif


int
ipx_strerror_fn(int errnum, char *buffer, size_t buffer_size)
{
    // Expecting XSI strerror_r version
    if (strerror_r(errnum, buffer, buffer_size) == 0) {
        return IPX_OK;
    }

    snprintf(buffer, buffer_size, "strerror_r() failed: Unable process error code %d!", errnum);
    return IPX_ERR_ARG;
}

int
ipx_utils_mkdir(const char *path, mode_t mode)
{
    const char ch_slash = '/';
    bool add_slash = false;

    // Check the parameter
    size_t len = strlen(path);
    if (path[len - 1] != ch_slash) {
        len++; // We have to add another slash
        add_slash = true;
    }

    if (len > PATH_MAX - 1) {
        errno = ENAMETOOLONG;
        return IPX_ERR_DENIED;
    }

    // Make a copy
    char *path_cpy = malloc((len + 1) * sizeof(char)); // +1 for '\0'
    if (!path_cpy) {
        errno = ENOMEM;
        return IPX_ERR_DENIED;
    }

    strcpy(path_cpy, path);
    if (add_slash) {
        path_cpy[len - 1] = ch_slash;
        path_cpy[len] = '\0';
    }

    struct stat info;
    char *pos;

    // Create directories from the beginning
    for (pos = path_cpy + 1; *pos; pos++) {
        // Find a slash
        if (*pos != ch_slash) {
            continue;
        }

        *pos = '\0'; // Temporarily truncate pathname

        // Check if a subdirectory exists
        if (stat(path_cpy, &info) == 0) {
            // Check if the "info" is about directory
            if (!S_ISDIR(info.st_mode)) {
                free(path_cpy);
                errno = ENOTDIR;
                return IPX_ERR_DENIED;
            }

            // Fix the pathname and continue with the next subdirectory
            *pos = ch_slash;
            continue;
        }

        // Errno is filled by stat()
        if (errno != ENOENT) {
            int errno_cpy = errno;
            free(path_cpy);
            errno = errno_cpy;
            return IPX_ERR_DENIED;
        }

        // Required directory doesn't exist -> create new one
        if (mkdir(path_cpy, mode) != 0 && errno != EEXIST) {
            // Failed (by the way, EEXIST because of race condition i.e.
            // multiple applications creating the same folder)
            int errno_cpy = errno;
            free(path_cpy);
            errno = errno_cpy;
            return IPX_ERR_DENIED;
        }

        *pos = ch_slash;
    }

    free(path_cpy);
    return IPX_OK;
}
