/**
 * \file src/core/fpipe.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Feedback pipe(source file)
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

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <errno.h>

#include "fpipe.h"
#include "utils.h"
#include "verbose.h"

/** Internal identification of the feedback pipe */
static const char *fpipe_str = "Feedback pipe";

/** \brief Parser feedback pipe */
struct ipx_fpipe {
    /** Pipe file descriptor for an input plugin that supports parser's feedback     */
    int fd_read;
    /** Pipe file descriptor for an IPFIX message parser                             */
    int fd_write;
    /** Control variable that will be set when a request is ready                    */
    atomic_flag sync_flag;
    /** Records in the pipe                                                          */
    volatile uint32_t cnt;

};

ipx_fpipe_t *
ipx_fpipe_create()
{
    struct ipx_fpipe *ret = malloc(sizeof(*ret));
    if (!ret) {
        return NULL;
    }

    int pipefd[2];
    if (pipe(pipefd) != 0) {
        // Failed
        free(ret);
        return NULL;
    }

    ret->fd_write = pipefd[1];
    ret->fd_read = pipefd[0];
    atomic_flag_clear(&ret->sync_flag);
    ret->cnt = 0;

    return ret;
}

void
ipx_fpipe_destroy(ipx_fpipe_t *fpipe)
{
    // Close the pipe and destroy the structure
    close(fpipe->fd_read);
    close(fpipe->fd_write);
    free(fpipe);
}

int
ipx_fpipe_write(ipx_fpipe_t *fpipe, const struct ipx_session *ts)
{
    assert(ts != NULL);

    // Send a pointer to the session (write of less than 4096 bytes is atomic - man 7 pipe)
    while (true) {
        ssize_t rc = write(fpipe->fd_write, &ts, sizeof(struct ipx_session *));
        if (rc > 0 && ((size_t) rc) == sizeof(struct ipx_session *)) {
            // Success
            break;
        }

        if (rc == -1 && errno == EINTR) {
            // Interrupted, try again
            continue;
        }

        if (rc == -1) {
            const char *err_str;
            ipx_strerror(errno, err_str);
            IPX_ERROR(fpipe_str, "Failed to write: %s", err_str);
        } else {
            IPX_ERROR(fpipe_str, "Invalid return code of write: %ld", rc);
        }

        return IPX_ERR_ARG;
    }

    // Success - spin until the lock is acquired
    while (atomic_flag_test_and_set_explicit(&fpipe->sync_flag, memory_order_acquire));
    fpipe->cnt++;
    atomic_flag_clear_explicit(&fpipe->sync_flag, memory_order_release);
    return IPX_OK;
}

int
ipx_fpipe_read(ipx_fpipe_t *fpipe, const struct ipx_session **ts)
{
    // Spin until the lock is acquired
    while (atomic_flag_test_and_set_explicit(&fpipe->sync_flag, memory_order_acquire));
    uint32_t cnt = fpipe->cnt;
    if (fpipe->cnt > 0) {
        fpipe->cnt--;
    }
    // Release the lock
    atomic_flag_clear_explicit(&fpipe->sync_flag, memory_order_release);

    if (cnt == 0) {
        return IPX_ERR_NOTFOUND;
    }

    // Read from the pipe
    struct ipx_session *ptr;
    void *buffer = &ptr;
    size_t buffer_size = sizeof(ptr);
    size_t read_size = 0;

    errno = 0;
    while (read_size < buffer_size) { // Read is not atomic and can be interrupted
        ssize_t rc = read(fpipe->fd_read, ((uint8_t *)buffer) + read_size, buffer_size - read_size);
        if (rc > 0) {
            read_size += (size_t) rc;
            continue;
        }

        if (rc < 0 && errno == EINTR) {
            // Try again
            continue;
        }

        // Unable to read
        if (rc == 0) {
            IPX_ERROR(fpipe_str, "End of file has been reached!", '\0');
        } else {
            const char *err_str;
            ipx_strerror(errno, err_str);
            IPX_ERROR(fpipe_str, "Failed to read: %s", err_str);
        }

        return IPX_ERR_ARG;
    }

    (*ts) = ptr;
    return IPX_OK;
}
