/**
 * @file
 * @author Adrian Duriska <xdurisa00@stud.fit.vutbr.cz>
 * @brief Periodic message
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stddef.h>
#include <assert.h>
#include <sys/time.h>

#include <ipfixcol2.h>
#include "message_periodic.h"

/**
 * \brief Structure of a periodic message
 */
struct ipx_msg_periodic {
    struct ipx_msg msg_header;
    /** Sequential number of the message */
    uint64_t seq;
    /** Timestamp, when the message was created */
    struct timespec created;
    /** Timestamp, when the message left the last intermediate plugin */
    struct timespec last_processed;
};

static_assert(offsetof(struct ipx_msg_periodic, msg_header.type) == 0,
    "Message header must be the first element of each IPFIXcol message.");

ipx_msg_periodic_t *
ipx_msg_periodic_create(uint64_t seq)
{
    struct ipx_msg_periodic *msg;

    msg = malloc(sizeof(*msg));
    if (!msg) {
        return NULL;
    }

    ipx_msg_header_init(&msg->msg_header, IPX_MSG_PERIODIC);
    clock_gettime(CLOCK_MONOTONIC, &msg->created);
    msg->last_processed = msg->created;
    msg->seq = seq;

    return msg;
}

void
ipx_msg_periodic_destroy(ipx_msg_periodic_t *msg)
{
    ipx_msg_header_destroy(&msg->msg_header);
    free(msg);
}

uint64_t
ipx_msg_periodic_get_seq_num(ipx_msg_periodic_t *msg)
{
    return msg->seq;
}

struct timespec
ipx_msg_periodic_get_created(ipx_msg_periodic_t *msg)
{
    return msg->created;
}

struct timespec
ipx_msg_periodic_get_last_processed(ipx_msg_periodic_t *msg)
{
    return msg->last_processed;
}

void
ipx_msg_periodic_update_last_processed(ipx_msg_periodic_t *msg)
{
    clock_gettime(CLOCK_MONOTONIC, &msg->last_processed);
}
