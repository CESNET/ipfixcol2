/**
 * \file src/plugins/intermediate/filter/msg_builder.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Helper for building new IPFIX messages
 * \date 2020
 */

/* Copyright (C) 2020 CESNET, z.s.p.o.
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

#ifndef MSG_BUILDER_H
#define MSG_BUILDER_H

#include <ipfixcol2.h>
#include <stdlib.h>
#include <libfds.h>

typedef struct {
    ipx_msg_ipfix_t *msg; // The message being builded

    uint8_t *buffer; // The raw message bytes
    size_t msg_len; // The number of bytes written so far

    struct fds_ipfix_set_hdr *current_set; // The current data set being created
} msg_builder_s;

/**
 * Write message bytes
 */
static inline void
msg_builder_write(msg_builder_s *b, const void *bytes, int count)
{
    memcpy(b->buffer + b->msg_len, bytes, count);
    b->msg_len += count;
}

/**
 * Initialize the message builder from the original message
 */
static inline int
msg_builder_init(msg_builder_s *b, ipx_ctx_t *ipx_ctx, ipx_msg_ipfix_t *orig_msg)
{
    // Allocate buffer same as the size of the original message because the new message cannot be bigger
    struct fds_ipfix_msg_hdr *orig_hdr = (struct fds_ipfix_msg_hdr *) ipx_msg_ipfix_get_packet(orig_msg);
    b->buffer = malloc(ntohs(orig_hdr->length));
    if (!b->buffer) {
        return IPX_ERR_NOMEM;
    }

    // Create new message from the original
    b->msg = ipx_msg_ipfix_create(ipx_ctx, ipx_msg_ipfix_get_ctx(orig_msg), b->buffer, 0);
    if (!b->msg) {
        free(b->buffer);
        return IPX_ERR_NOMEM;
    }

    // Initialize builder fields
    b->msg_len = 0;

    // Copy the original header
    msg_builder_write(b, orig_hdr, sizeof(struct fds_ipfix_msg_hdr));

    return IPX_OK;
}

/**
 * Copy ipfix set from the original
 */
static inline int
msg_builder_copy_set(msg_builder_s *b, struct ipx_ipfix_set *set)
{
    struct ipx_ipfix_set *ref = ipx_msg_ipfix_add_set_ref(b->msg);
    if (!ref) {
        return IPX_ERR_NOMEM;
    }
    ref->ptr = (struct fds_ipfix_set_hdr *) (b->buffer + b->msg_len);
    msg_builder_write(b, set->ptr, ntohs(set->ptr->length));
    return IPX_OK;
}

/**
 * Begin new data set
 */
static inline void
msg_builder_begin_dset(msg_builder_s *b, uint16_t flowset_id)
{
    b->current_set = (struct fds_ipfix_set_hdr *) (b->buffer + b->msg_len);
    const struct fds_ipfix_set_hdr hdr = { .flowset_id = htons(flowset_id) };
    msg_builder_write(b, &hdr, sizeof(hdr));
}

/**
 * Copy data record to the message
 */
static inline int
msg_builder_copy_drec(msg_builder_s *b, struct ipx_ipfix_record *drec)
{
    struct ipx_ipfix_record *ref = ipx_msg_ipfix_add_drec_ref(&b->msg);
    if (!ref) {
        return IPX_ERR_NOMEM;
    }
    memcpy(&ref->rec, &drec->rec, sizeof(struct fds_drec));
    ref->rec.data = b->buffer + b->msg_len;
    msg_builder_write(b, drec->rec.data, drec->rec.size);
    return IPX_OK;
}

/**
 * End the current data set
 */
static inline int
msg_builder_end_dset(msg_builder_s *b)
{
    uint16_t set_len = (uint8_t *) (b->buffer + b->msg_len) - (uint8_t *) b->current_set;
    if (set_len <= sizeof(struct fds_ipfix_set_hdr)) {
        // No data records written, don't even write the set header, rewind the buffer to the set start
        b->msg_len -= set_len;
        return IPX_OK;
    }

    b->current_set->length = htons(set_len);
    struct ipx_ipfix_set *ref = ipx_msg_ipfix_add_set_ref(b->msg);
    if (!ref) {
        return IPX_ERR_NOMEM;
    }
    // b->current_set is a pointer to the beginning of the data set
    ref->ptr = b->current_set;
    return IPX_OK;
}

/**
 * Finish the ipfix message
 */
static inline void
msg_builder_finish(msg_builder_s *b)
{
    // Set the correct message length in the message header
    struct fds_ipfix_msg_hdr *hdr = (struct fds_ipfix_msg_hdr *) b->buffer;
    hdr->length = htons(b->msg_len);
    ipx_msg_ipfix_set_raw_size(b->msg, b->msg_len);
}

/**
 * Check if the message is empty meaning nothing was written except the header
 */
static inline bool
msg_builder_is_empty_msg(msg_builder_s *b)
{
    return b->msg_len <= sizeof(struct fds_ipfix_msg_hdr);
}

#endif
