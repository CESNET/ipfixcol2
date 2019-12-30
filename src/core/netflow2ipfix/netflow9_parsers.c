/**
 * \file src/core/netflow2ipfix/netflow_parsers.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief NetFlow v9 parsers (source file)
 * \date 2018-2019
 *
 * Copyright(c) 2019 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <limits.h>
#include <assert.h>
#include "netflow9_parsers.h"

/// Internal error codes
enum error_codes {
    // No error
    ERR_OK,
    // FlowSets iterator
    ERR_SETS_UEND,
    ERR_SETS_SHORT,
    ERR_SETS_LONG,
    // Data FlowSet iterator
    ERR_DSET_EMPTY,
    // (Options) Template FlowSet iterator
    ERR_TSET_EMPTY,
    ERR_TSET_SOPTS,
    ERR_TSET_SFIELD,
    ERR_TSET_TID,
    ERR_TSET_CNT,
    ERR_TSET_END,
    ERR_TSET_ZERO,
    ERR_TSET_DATA
};

/// Error messages
static const char *err_msg[] = {
    [ERR_OK]          = "No error.",
    // FlowSets iterator
    [ERR_SETS_UEND]   = "The NetFlow v9 Message size is invalid (unexpected end of the message).",
    [ERR_SETS_SHORT]  = "Total length of the FlowSet is shorter than a length of a NetFlow v9 "
        "FlowSet header.",
    [ERR_SETS_LONG]   = "Total length of the FlowSet is longer than its enclosing NetFlow v9 "
        "Message.",
    // Data FlowSet iterator
    [ERR_DSET_EMPTY]  = "A DataFlow Set is empty or contains a malformed record (shorted that "
        "described in its particular template). At least one valid record must be present.",
    // (Options) Template FlowSet iterator
    [ERR_TSET_EMPTY]  = "An (Options) Template FlowSet must not be empty. At least one record must "
        "be present.",
    [ERR_TSET_SOPTS]  = "Invalid Options Scope length in an Options Template definition.",
    [ERR_TSET_SFIELD] = "Invalid Option Length in an Options Template definition.",
    [ERR_TSET_TID]    = "Template ID of an (Options) Template is invalid (< 256).",
    [ERR_TSET_CNT]    = "An (Options) Template without field definitions is not valid.",
    [ERR_TSET_END]    = "Invalid definition of an (Options) Template (unexpected end of the "
        "(Options) Template FlowSet).",
    [ERR_TSET_ZERO]   = "An (Options) Template defines a prohibited zero length Data Record.",
    [ERR_TSET_DATA]   = "An (Options) Template defines a Data Record which length exceeds "
        "the maximum length of a Data Record that fits into a NetFlow v9 Message.",
};


void
ipx_nf9_sets_iter_init(struct ipx_nf9_sets_iter *it, const struct ipx_nf9_msg_hdr *nf9_msg,
    uint16_t nf9_size)
{
    it->_private.set_next = ((const uint8_t *) nf9_msg) + IPX_NF9_MSG_HDR_LEN;
    it->_private.msg_end = ((const uint8_t *) nf9_msg) + nf9_size;
    assert(it->_private.set_next <= it->_private.msg_end);
    it->_private.err_msg = err_msg[ERR_OK];
}

int
ipx_nf9_sets_iter_next(struct ipx_nf9_sets_iter *it)
{
    if (it->_private.set_next == it->_private.msg_end) {
        return IPX_EOC;
    }

    // Make sure that the iterator is not beyond the end of the message
    assert(it->_private.set_next < it->_private.msg_end);

    // Check the remaining length of the NetFlow message
    if (it->_private.set_next + IPX_NF9_SET_HDR_LEN > it->_private.msg_end) {
        // Unexpected end of the NetFlow message
        it->_private.err_msg = err_msg[ERR_SETS_UEND];
        return IPX_ERR_FORMAT;
    }

    const struct ipx_nf9_set_hdr *new_set = (const struct ipx_nf9_set_hdr *) it->_private.set_next;
    uint16_t set_len = ntohs(new_set->length);

    // Check the length of the FlowSet
    if (set_len < IPX_NF9_SET_HDR_LEN) {
        // Length of a FlowSet is shorted that an NetFlow FlowSet header
        it->_private.err_msg = err_msg[ERR_SETS_SHORT];
        return IPX_ERR_FORMAT;
    }

    if (it->_private.set_next + set_len > it->_private.msg_end) {
        // Length of the FlowSet is longer than its enclosing NetFlow Message
        it->_private.err_msg = err_msg[ERR_SETS_LONG];
        return IPX_ERR_FORMAT;
    }

    it->_private.set_next += set_len;
    it->set = new_set;
    return IPX_OK;
}

const char *
ipx_nf9_sets_iter_err(const struct ipx_nf9_sets_iter *it)
{
    return it->_private.err_msg;
}

// -------------------------------------------------------------------------------------------------

/// Internal iterator flags
enum ipx_nf9_dset_iter_flags {
    /// Initialization fail and an error message is set
    IPX_NF9_DSET_ITER_FAILED = (1 << 0)
};

void
ipx_nf9_dset_iter_init(struct ipx_nf9_dset_iter *it, const struct ipx_nf9_set_hdr *set,
    uint16_t rec_size)
{
    const uint16_t set_id = ntohs(set->flowset_id);
    const uint16_t set_len = ntohs(set->length);
    assert(set_id >= IPX_NF9_SET_MIN_DSET);
    assert(set_len >= IPX_NF9_SET_HDR_LEN);

    it->_private.flags = 0;
    it->_private.rec_size = rec_size;
    it->_private.rec_next = ((const uint8_t *) set) + IPX_NF9_SET_HDR_LEN;
    it->_private.set_end = ((const uint8_t *) set) + set_len;
    it->_private.err_msg = err_msg[ERR_OK];

    if (it->_private.rec_next + rec_size > it->_private.set_end) {
        // Empty set is not valid (see RFC 3954, Section 2, Data FlowSet)
        it->_private.flags |= IPX_NF9_DSET_ITER_FAILED;
        it->_private.err_msg = err_msg[ERR_DSET_EMPTY];
    }
}

int
ipx_nf9_dset_iter_next(struct ipx_nf9_dset_iter *it)
{
    if ((it->_private.flags & IPX_NF9_DSET_ITER_FAILED) != 0) {
        // Initialization failed, error code is properly set
        return IPX_ERR_FORMAT;
    }

    if (it->_private.rec_next + it->_private.rec_size > it->_private.set_end) {
        // The iterator reached the end of the Data Set or the rest is padding
        return IPX_EOC;
    }

    it->rec = it->_private.rec_next;
    it->_private.rec_next += it->_private.rec_size;
    return IPX_OK;
}

const char *
ipx_nf9_dset_iter_err(const struct ipx_nf9_dset_iter *it)
{
    return it->_private.err_msg;
}

// -------------------------------------------------------------------------------------------------

/// Internal iterator flags
enum ipx_nf9_tset_iter_flags {
    /// Initialization fail and an error message is set
    IPX_NF9_TSET_ITER_FAILED = (1 << 0)
};

void
ipx_nf9_tset_iter_init(struct ipx_nf9_tset_iter *it, const struct ipx_nf9_set_hdr *set)
{
    const uint16_t set_id = ntohs(set->flowset_id);
    const uint16_t set_len = ntohs(set->length);

    assert(set_id == IPX_NF9_SET_TMPLT || set_id == IPX_NF9_SET_OPTS_TMPLT);
    assert(set_len >= IPX_NF9_SET_HDR_LEN);

    it->_private.type     = set_id;
    it->_private.flags    = 0;
    it->_private.rec_next = ((const uint8_t *) set) + IPX_NF9_SET_HDR_LEN;
    it->_private.set_end  = ((const uint8_t *) set) + set_len;
    it->_private.err_msg  = err_msg[ERR_OK];

    // Minimal size of a definition is a template with just one field (4B + 4B, resp. 6B + 4B)
    const uint16_t min_size = (set_id == IPX_NF9_SET_TMPLT) ? 8U : 10U;
    if (it->_private.rec_next + min_size > it->_private.set_end) {
        // Empty (Options) Template FlowSet is not valid (see RFC 3954, Section 2, Template FlowSet
        it->_private.flags |= IPX_NF9_TSET_ITER_FAILED;
        it->_private.err_msg = err_msg[ERR_TSET_EMPTY];
        return;
    }
}

int
ipx_nf9_tset_iter_next(struct ipx_nf9_tset_iter *it)
{
    if ((it->_private.flags & IPX_NF9_TSET_ITER_FAILED) != 0) {
        // Initialization failed, error code is properly set
        return IPX_ERR_FORMAT;
    }

    // Minimal size of a definition is a template with just one field (4B + 4B, resp. 6B + 4B)
    const uint16_t min_size = (it->_private.type == IPX_NF9_SET_TMPLT) ? 8U : 10U;
    if (it->_private.rec_next + min_size > it->_private.set_end) {
        // End of the FlowSet or padding
        return IPX_EOC;
    }

    uint16_t tmplt_id;
    uint16_t field_cnt;
    uint16_t scope_cnt = 0;
    uint32_t data_size = 0;
    const struct ipx_nf9_tmplt_ie *field_ptr;

    if (it->_private.type == IPX_NF9_SET_TMPLT) {
        // Template Record
        const struct ipx_nf9_trec *rec = (const struct ipx_nf9_trec *) it->_private.rec_next;
        tmplt_id = ntohs(rec->template_id);
        field_cnt = ntohs(rec->count);
        field_ptr = &rec->fields[0];
    } else {
        // Options Template Record
        const struct ipx_nf9_opts_trec *rec = (struct ipx_nf9_opts_trec *) it->_private.rec_next;
        tmplt_id = ntohs(rec->template_id);
        uint16_t size_opts = ntohs(rec->scope_length);
        uint16_t size_fields = ntohs(rec->option_length);

        if (size_opts % IPX_NF9_TMPLT_IE_LEN != 0) {
            it->_private.err_msg = err_msg[ERR_TSET_SOPTS];
            return IPX_ERR_FORMAT;
        }

        if (size_fields % IPX_NF9_TMPLT_IE_LEN != 0) {
            it->_private.err_msg = err_msg[ERR_TSET_SFIELD];
            return IPX_ERR_FORMAT;
        }

        scope_cnt = size_opts / IPX_NF9_TMPLT_IE_LEN;
        field_cnt = scope_cnt + (size_fields / IPX_NF9_TMPLT_IE_LEN);
        field_ptr = &rec->fields[0];
    }

    if (tmplt_id < IPX_NF9_SET_MIN_DSET) {
        // Invalid Template ID
        it->_private.err_msg = err_msg[ERR_TSET_TID];
        return IPX_ERR_FORMAT;
    }

    if (field_cnt == 0) {
        // The template is empty!
        it->_private.err_msg = err_msg[ERR_TSET_CNT];
        return IPX_ERR_FORMAT;
    }

    const uint8_t *tmptl_end = (const uint8_t *) (field_ptr + field_cnt);
    if (tmptl_end > it->_private.set_end) {
        // Unexpected end of the Set
        it->_private.err_msg = err_msg[ERR_TSET_END];
        return IPX_ERR_FORMAT;
    }

    // Calculate size of the corresponding Data record
    for (size_t i = 0; i < field_cnt; ++i, ++field_ptr) {
        data_size += ntohs(field_ptr->length);
    }

    if (data_size == 0) {
        // Template definition that describes zero-length records doesn't make sense
        it->_private.err_msg = err_msg[ERR_TSET_ZERO];
        return IPX_ERR_FORMAT;
    }

    // Maximum size of a data record (Max. Msg. length - NetFlow Msg. header - NetFlow Set header)
    const size_t data_max = UINT16_MAX - IPX_NF9_MSG_HDR_LEN - IPX_NF9_SET_HDR_LEN;
    if (data_size > data_max) {
        it->_private.err_msg = err_msg[ERR_TSET_DATA];
        return IPX_ERR_FORMAT;
    }

    // Everything seems ok...
    if (it->_private.type == IPX_NF9_SET_TMPLT) {
        it->ptr.trec = (const struct ipx_nf9_trec *) it->_private.rec_next;
    } else {
        it->ptr.opts_trec = (const struct ipx_nf9_opts_trec *) it->_private.rec_next;
    }

    const uint16_t tmplt_size = (uint16_t) (tmptl_end - it->_private.rec_next);
    it->size = tmplt_size;
    it->field_cnt = field_cnt;
    it->scope_cnt = scope_cnt;
    it->_private.rec_next += tmplt_size;
    return IPX_OK;
}

const char *
ipx_nf9_tset_iter_err(const struct ipx_nf9_tset_iter *it)
{
    return it->_private.err_msg;
}
