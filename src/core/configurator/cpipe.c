/**
 * @file src/core/configurator/cpipe.c
 * @author Lukas Hutak <lukas.hutak@cesnet.cz>
 * @brief Configuration request pipe (source file)
 * @date 2019
 *
 * Copyright(c) 2020 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h> // PIPE_BUF

#include <ipfixcol2.h>

#include "cpipe.h"
#include "../verbose.h"

/// Invalid file descriptor value
#define INVALID_FD (-1)
/// Configuration pipe - cpipe_fd[0] for read, cpipe_fd[1] for write
static int cpipe_fd[2] = {INVALID_FD, INVALID_FD};
/// Identification of the module (just for log!)
static const char *module = "Configuration pipe";

// Size of the request must allow atomic write! (see write() for help)
static_assert(sizeof(struct ipx_cpipe_req) <= PIPE_BUF, "non-atomic write!");

int
ipx_cpipe_init()
{
    assert(cpipe_fd[0] == INVALID_FD && cpipe_fd[1] == INVALID_FD && "Already initialized!");
    int rc;
    const char *err_str;

    // Create a pipe
    rc = pipe(cpipe_fd);
    if (rc != 0) {
        ipx_strerror(errno, err_str);
        IPX_ERROR(module, "pipe() failed: %s", err_str);
        return IPX_ERR_DENIED;
    }

    // Make write end non-blocking
    int flags = fcntl(cpipe_fd[1], F_GETFL);
    if (flags == -1) {
        ipx_strerror(errno, err_str);
        IPX_ERROR(module, "fcntl(..., F_GETFL) failed: %s", err_str);
        ipx_cpipe_destroy();
        return IPX_ERR_DENIED;
    }
    flags |= O_NONBLOCK;
    if (fcntl(cpipe_fd[1], F_SETFL, flags) == -1) {
        ipx_strerror(errno, err_str);
        IPX_ERROR(module, "fcntl(..., F_SETFL) failed: %s", err_str);
        ipx_cpipe_destroy();
        return IPX_ERR_DENIED;
    }

    return IPX_OK;
}

void
ipx_cpipe_destroy()
{
    for (int i = 0; i < 2; ++i) {
        if (cpipe_fd[i] == INVALID_FD) {
            continue;
        }

        close(cpipe_fd[i]);
        cpipe_fd[i] = INVALID_FD;
    }
}

int
ipx_cpipe_receive(struct ipx_cpipe_req *msg)
{
    const size_t buffer_size = sizeof(*msg);
    size_t buffer_read = 0;

    errno = 0;
    while (buffer_read < buffer_size) {
        uint8_t *ptr = ((uint8_t *) msg) + buffer_read;
        ssize_t rc = read(cpipe_fd[0], ptr, buffer_size - buffer_read);
        if (rc > 0) {
            buffer_read += (size_t) rc;
            continue;
        }

        if (rc == -1 && errno == EINTR) {
            // Try again
            continue;
        }

        // Unable to read
        if (rc == 0) {
            IPX_ERROR(module, "read() failed (write end-point is probably closed)", '\0');
        } else {
            const char *err_str;
            ipx_strerror(errno, err_str);
            IPX_ERROR(module, "read() failed: %s", err_str);
        }

        return IPX_ERR_DENIED;
    }

    return IPX_OK;
}

int
ipx_cpipe_send_term(ipx_ctx_t *ctx, enum ipx_cpipe_type type)
{
    // WARNING: Keep on mind that this function can be called from signal handler!

    // In case we change 'errno' (e.g. write())
    int errno_backup = errno;

    if (type != IPX_CPIPE_TYPE_TERM_SLOW
            && type != IPX_CPIPE_TYPE_TERM_FAST
            && type != IPX_CPIPE_TYPE_TERM_DONE) {
        return IPX_ERR_ARG;
    }

    // Prepare a request
    struct ipx_cpipe_req req;
    memset(&req, 0, sizeof(req));
    req.type = type;
    req.ctx = ctx;

    // Send it
    int rc = write(cpipe_fd[1], &req, sizeof(req));
    // Just in case, write of this size is atomic so following must be always true
    assert((rc == -1 || rc == sizeof(req)) && "Non-atomic write() is not allowed!");

    errno = errno_backup;
    return (rc == -1) ? IPX_ERR_DENIED : IPX_OK;
}

