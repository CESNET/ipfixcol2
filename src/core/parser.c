/**
 * \file src/core/parser.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief IPFIX Message parser (source file)
 * \date 2018-2019
 */

/* Copyright (C) 2018-2019 CESNET, z.s.p.o.
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

#include <stdlib.h>
#include <inttypes.h>
#include <libfds.h>
#include <ipfixcol2.h>

#include "message_ipfix.h"
#include "context.h"
#include "parser.h"
#include "verbose.h"
#include "fpipe.h"
#include "netflow2ipfix/netflow2ipfix.h"
#include "netflow2ipfix/netflow_structs.h"

/** Default record of the parser structure */
#define PARSER_DEF_RECS 8
/** Default record of the stream structure */
#define STREAM_DEF_RECS 1

/** Auxiliary flags specific to each Stream ID within a Stream context */
enum stream_info_flags {
    /** Stream has been seen                                           */
    SIF_SEEN = (1 << 0)
};

/** Parameters of a specific stream                                    */
struct stream_info {
    /** Stream ID                                                      */
    ipx_stream_t id;
    /** Stream flags (see #stream_info_flags)                          */
    uint16_t flags;
    /** Expected Sequence Number of the next Message                   */
    uint32_t seq_num;
};

/** Auxiliary flags specific to each Stream context                    */
enum stream_ctx_flags {
    /** Ignore all IPFIX messages                                      */
    SCF_BLOCK = (1 << 0),
};

/** Type of source data                                                */
enum source_type {
    /// Unknown type of messages
    ST_UNKNOWN,
    /// IPFIX Messages
    ST_IPFIX,
    /// NetFlow v5 Messages
    ST_NETFLOW5,
    /// NetFlow v9 Messages
    ST_NETFLOW9
};

/**
 * \brief Stream context
 * \note Represents parameters common to all streams within the same combination of Transport
 *   Session and ODID.
 */
struct stream_ctx {
    /** Template manager                          */
    fds_tmgr_t *mgr;
    /** Connection flags (see #stream_ctx_flags)  */
    uint16_t flags;
    /** Type of source messages (IPFIX/NetFlow)   */
    enum source_type type;
    /** Messages converters to IPFIX (based on source type) */
    union {
        /** Converter from NetFlow v5 to IPFIX    */
        ipx_nf5_conv_t *nf5;
        /** Converter from NetFlow v9 to IPFIX    */
        ipx_nf9_conv_t *nf9;
    } converter;

    /** Number of pre-allocated stream records    */
    size_t infos_alloc;
    /** Number of valid stream records            */
    size_t infos_valid;
    /** Array of sorted information about Streams */
    struct stream_info infos[1];
};

/**
 * \brief Parser record
 * \note Record represents unique combination of Transport Session and ODID.
 * */
struct parser_rec {
    /** Transport Session                         */
    const struct ipx_session *session;
    /** Observation Domain ID                     */
    uint32_t odid;

    /** Context common for all streams            */
    struct stream_ctx *ctx;
};

/** Main structure of IPFIX message parser         */
struct ipx_parser {
    /** Plugin identification (for logs)           */
    char *ident;
    /** Verbosity level of the parser              */
    enum ipx_verb_level vlevel;

    /** Source of Information Elements             */
    const struct fds_iemgr *ie_mgr;

    /** Number of pre-allocated records            */
    size_t recs_alloc;
    /** Number of valid records                    */
    size_t recs_valid;
    /** Array of records                           */
    struct parser_rec *recs;
};

/**
 * \def PARSER_ERROR
 * \brief Macro for printing an error message of a parser
 * \param[in] parser  Parser
 * \param[in] msg_ctx IPFIX Message context (Transport Session, ODID, ...)
 * \param[in] fmt     Format string (see manual page for "printf" family)
 * \param[in] ...     Variable number of arguments for the format string
 */
#define PARSER_ERROR(parser, msg_ctx, fmt, ...)                                                  \
    if ((parser)->vlevel >= IPX_VERB_ERROR) {                                                    \
        ipx_verb_print(IPX_VERB_ERROR, "ERROR: %s: [%s, ODID: %" PRIu32 "] " fmt "\n",           \
            (parser)->ident, (msg_ctx)->session->ident, (msg_ctx)->odid, ## __VA_ARGS__);        \
    }

/**
 * \def PARSER_WARNING
 * \brief Macro for printing a warning message of a parser
 * \param[in] parser  Parser
 * \param[in] msg_ctx IPFIX Message context (Transport Session, ODID, ...)
 * \param[in] fmt     Format string (see manual page for "printf" family)
 * \param[in] ...     Variable number of arguments for the format string
 */
#define PARSER_WARNING(parser, msg_ctx, fmt, ...) \
    if ((parser)->vlevel >= IPX_VERB_WARNING) {                                                  \
        ipx_verb_print(IPX_VERB_WARNING, "WARNING: %s: [%s, ODID: %" PRIu32 "] " fmt "\n",       \
            (parser)->ident, (msg_ctx)->session->ident, (msg_ctx)->odid, ## __VA_ARGS__);        \
    }

/**
 * \def PARSER_INFO
 * \brief Macro for printing an info message of a parser
 * \param[in] parser  Parser
 * \param[in] msg_ctx IPFIX Message context (Transport Session, ODID, ...)
 * \param[in] fmt     Format string (see manual page for "printf" family)
 * \param[in] ...     Variable number of arguments for the format string
 */
#define PARSER_INFO(parser, msg_ctx, fmt, ...) \
    if ((parser)->vlevel >= IPX_VERB_INFO) {                                                     \
        ipx_verb_print(IPX_VERB_INFO, "INFO: %s: [%s, ODID: %" PRIu32 "] " fmt "\n",             \
            (parser)->ident, (msg_ctx)->session->ident, (msg_ctx)->odid, ## __VA_ARGS__);        \
    }

/**
 * \def PARSER_DEBUG
 * \brief Macro for printing a debug message of a parser
 * \param[in] parser  Parser
 * \param[in] msg_ctx IPFIX Message context (Transport Session, ODID, ...)
 * \param[in] fmt     Format string (see manual page for "printf" family)
 * \param[in] ...     Variable number of arguments for the format string
 */
#define PARSER_DEBUG(parser, msg_ctx, fmt, ...) \
    if ((parser)->vlevel >= IPX_VERB_DEBUG) {                                                    \
        ipx_verb_print(IPX_VERB_DEBUG, "DEBUG: %s: [%s, ODID: %" PRIu32 "] " fmt "\n",           \
            (parser)->ident, (msg_ctx)->session->ident, (msg_ctx)->odid, ## __VA_ARGS__);        \
    }

/**
 * \def STREAM_CTX_SIZE
 * \brief Get size of stream_ctx structure suitable for \p recs records
 */
#define STREAM_CTX_SIZE(recs)    \
    sizeof(struct stream_ctx)    \
    - sizeof(struct stream_info) \
    + ((recs) * sizeof(struct stream_info))

/**
 * \brief Create a new stream context
 * \return Pointer or NULL (memory allocation error)
 */
static struct stream_ctx *
stream_ctx_create(const struct ipx_parser *parser, const struct ipx_session *session)
{
    struct stream_ctx *ctx = calloc(1, STREAM_CTX_SIZE(STREAM_DEF_RECS));
    if (!ctx) {
        return NULL;
    }
    ctx->infos_alloc = STREAM_DEF_RECS;
    ctx->infos_valid = 0;
    ctx->flags = 0;
    ctx->type = ST_UNKNOWN; // type of flows is unknown

    // Initialize a new template manager
    ctx->mgr = fds_tmgr_create(session->type);
    if (!ctx->mgr) {
        free(ctx);
        return NULL;
    }

    if (session->type == FDS_SESSION_UDP) {
        int rc = fds_tmgr_set_udp_timeouts(ctx->mgr, session->udp.lifetime.tmplts,
            session->udp.lifetime.opts_tmplts);
        // Just session type must be correct
        assert(rc == FDS_OK);
    }

    // Define source of Information Elements
    if (fds_tmgr_set_iemgr(ctx->mgr, parser->ie_mgr) != FDS_OK) {
        fds_tmgr_destroy(ctx->mgr);
        free(ctx);
        return NULL;
    }

    return ctx;
}

/**
 * \brief Destroy a stream context
 * \warning Template manager and all templates within will be freed too.
 * \param[in] ctx Steam context
 */
static void
stream_ctx_destroy(struct stream_ctx *ctx)
{
    fds_tmgr_destroy(ctx->mgr);

    // Destroy converters
    if (ctx->type == ST_NETFLOW5 && ctx->converter.nf5 != NULL) {
        ipx_nf5_conv_destroy(ctx->converter.nf5);
    }
    if (ctx->type == ST_NETFLOW9 && ctx->converter.nf9 != NULL) {
        ipx_nf9_conv_destroy(ctx->converter.nf9);
    }

    free(ctx);
}

/**
 * \brief Compare two stream_info structures
 * \param[in] p1 First structure
 * \param[in] p2 Second structure
 * \return An integer less than, equal to, or greater than zero if the first argument is considered
 *   to be respectively less than, equal to, or greater than the second.
 */
static int
stream_ctx_rec_cmp(const void *p1, const void *p2)
{
    const struct stream_info *stream_l = p1;
    const struct stream_info *stream_r = p2;

    if (stream_l->id == stream_r->id) {
        return 0;
    } else {
        return (stream_l->id < stream_r->id) ? (-1) : 1;
    }
}

/**
 * \brief Sort an array of records within a stream_ctx structure
 * \param[in] ctx Stream context
 */
static void
stream_ctx_rec_sort(struct stream_ctx *ctx)
{
    const size_t rec_size = sizeof(*ctx->infos);
    const size_t rec_cnt = ctx->infos_valid;
    qsort(ctx->infos, rec_cnt, rec_size, &stream_ctx_rec_cmp);
}

/**
 * \brief Find a stream_info record defined by Stream ID within a stream context
 * \param[in] ctx Stream context structure
 * \param[in] id  Stream ID
 * \return On success returns a pointer to the structure. Otherwise the required record is not
 *   present.
 */
static struct stream_info *
stream_ctx_rec_find(struct stream_ctx *ctx, ipx_stream_t id)
{
    if (ctx->infos_valid == 0) {
        return NULL;
    }

    // TCP and UDP works only with stream 0 and the array is always sorted...
    if (ctx->infos[0].id == id) {
        return &ctx->infos[0];
    }

    // Find manually
    const size_t rec_size = sizeof(*ctx->infos);
    const size_t rec_cnt = ctx->infos_valid;

    struct stream_info key;
    key.id = id;
    return bsearch(&key, ctx->infos, rec_cnt, rec_size, &stream_ctx_rec_cmp);
}

/**
 * \brief Get a stream_info record defined by Stream ID within a stream context
 *
 * The function will try to find and return the required record. If the record doesn't exist
 * a new one will be created.
 * \warning Because the context can be reallocated, the context pointer \p ctx can be changed.
 * \param[in,out] ctx Stream context structure
 * \param[in]     id  Stream ID
 * \return Pointer or NULL (memory allocation error)
 */
static struct stream_info *
stream_ctx_rec_get(struct stream_ctx **ctx, ipx_stream_t id)
{
    // Try to find the record
    struct stream_info *info = stream_ctx_rec_find(*ctx, id);
    if (info != NULL) {
        return info;
    }

    // Add a new record
    if ((*ctx)->infos_valid == (*ctx)->infos_alloc) {
        const size_t alloc_new = 2 * (*ctx)->infos_alloc;
        const size_t alloc_size = STREAM_CTX_SIZE(alloc_new);
        struct stream_ctx *ctx_new = realloc(*ctx, alloc_size);
        if (!ctx_new) {
            return NULL;
        }

        ctx_new->infos_alloc = alloc_new;
        *ctx = ctx_new;
    }

    // Create a new record, sort and return the record
    info = &(*ctx)->infos[(*ctx)->infos_valid++];
    info->id = id;
    info->seq_num = 0;
    info->flags = 0;

    stream_ctx_rec_sort(*ctx);
    // Position has been changed... find it again
    info = stream_ctx_rec_find(*ctx, id);
    assert(info != NULL);
    return info;
}

/**
 * \brief Compare two parser_rec structures
 * \param[in] rec1 First structure
 * \param[in] rec2 Second structure
 * \return An integer less than, equal to, or greater than zero if the first argument is considered
 *   to be respectively less than, equal to, or greater than the second.
 */
static int
parser_rec_cmp(const void *rec1, const void *rec2)
{
    const struct parser_rec *rec_l = rec1;
    const struct parser_rec *rec_r = rec2;

    // Array MUST be sorted primary by Transport Session
    if (rec_l->session != rec_r->session) {
        return (rec_l->session < rec_r->session) ? (-1) : 1;
    }

    if (rec_l->odid == rec_r->odid) {
        return 0;
    }

    return (rec_l->odid < rec_r->odid) ? (-1) : 1;
}

/**
 * \brief Sort an array of parser records
 * \param parser Parser structure
 */
static void
parser_rec_sort(struct ipx_parser *parser)
{
    const size_t rec_cnt = parser->recs_valid;
    const size_t rec_size = sizeof(*parser->recs);
    qsort(parser->recs, rec_cnt, rec_size, &parser_rec_cmp);
}

/**
 * \brief Find a parser record defined by Transport Session and Observation Domain ID within
 *   a parser
 * \param[in] parser Parser structure
 * \param[in] ctx    IPFIX Message context (info about Transport Session, ODID)
 * \return On success returns a pointer to the structure. Otherwise the required record is not
 *   present.
 */
static struct parser_rec *
parser_rec_find(struct ipx_parser *parser, const struct ipx_msg_ctx *ctx)
{
    const size_t rec_cnt = parser->recs_valid;
    const size_t rec_size = sizeof(*parser->recs);
    struct parser_rec key;
    key.session = ctx->session;
    key.odid = ctx->odid;

    return bsearch(&key, parser->recs, rec_cnt, rec_size, &parser_rec_cmp);
}

/**
 * \brief Get a parser record defined by Transport Session and Observation Domain ID within
 *   a parser
 *
 * The function will try to find and return the required record. If the record doesn't exist
 * a new one will be created.
 * \param[in] parser Parser structure
 * \param[in] ctx    IPFIX Message context (info about Transport Session, ODID)
 * \return Pointer or NULL (memory allocation error)
 */
static struct parser_rec *
parser_rec_get(struct ipx_parser *parser, const struct ipx_msg_ctx *ctx)
{
    // Try to find a record first
    struct parser_rec *rec = parser_rec_find(parser, ctx);
    if (rec != NULL) {
        return rec;
    }

    if (parser->recs_valid == parser->recs_alloc) {
        const size_t alloc_new = 2 * parser->recs_alloc;
        const size_t alloc_size = alloc_new * sizeof(*parser->recs);
        struct parser_rec *recs_new = realloc(parser->recs, alloc_size);
        if (!recs_new) {
            return NULL;
        }

        parser->recs = recs_new;
        parser->recs_alloc = alloc_new;
    }

    // Add a new record, sort and return it
    rec = &parser->recs[parser->recs_valid];
    rec->session = ctx->session;
    rec->odid = ctx->odid;
    rec->ctx = stream_ctx_create(parser, ctx->session);
    if (!rec->ctx) {
        return NULL;
    }

    PARSER_INFO(parser, ctx, "New connection detected!", '\0');

    parser->recs_valid++;
    parser_rec_sort(parser);
    // Position has been changes... find it again
    rec = parser_rec_find(parser, ctx);
    assert(rec != NULL);
    return rec;
}

/** Auxiliary structure for garbage after Transport Session removal */
struct session_gabage {
    /** Number of records */
    size_t rec_cnt;
    /** Array of records to remove  */
    struct stream_ctx **recs;
};

/**
 * \brief Free all streams and their template managers
 * \param[in] grb Garbage to destroy
 */
static void
session_garbage_destroy(struct session_gabage *grb)
{
    if (grb == NULL) {
        return;
    }

    for (size_t i = 0; i < grb->rec_cnt; ++i) {
        stream_ctx_destroy(grb->recs[i]);
    }

    free(grb->recs);
    free(grb);
}

/**
 * \brief Create a garbage message with selected parser records
 *
 * \warning Selected records remains in the parser. They must be removed manually.
 * \param[in] parser Parser
 * \param[in] begin  Index of the first record to be included
 * \param[in] end    Index of "past-the-last" record to be included
 * \return Pointer or NULL (memory allocation error)
 */
static ipx_msg_garbage_t *
parser_rec_to_garbage(struct ipx_parser *parser, size_t begin, size_t end)
{
    assert(begin < end);

    // Prepare data structures
    struct session_gabage *garbage = malloc(sizeof(*garbage));
    if (!garbage) {
        return NULL;
    }

    garbage->rec_cnt = end - begin;
    garbage->recs = malloc(garbage->rec_cnt * sizeof(*garbage->recs));
    if (!garbage->recs) {
        free(garbage);
        return NULL;
    }

    // Copy records
    for (size_t idx = begin, pos = 0; idx < end; ++idx, ++pos) {
        assert(pos < garbage->rec_cnt);
        garbage->recs[pos] = parser->recs[idx].ctx;
    }

    // Wrap the garbage
    ipx_msg_garbage_cb cb = (ipx_msg_garbage_cb) &session_garbage_destroy;
    ipx_msg_garbage_t *msg = ipx_msg_garbage_create(garbage, cb);
    if (!msg) {
        free(garbage->recs);
        free(garbage);
        return NULL;
    }

    return msg;
}

/**
 * \brief Compare sequence numbers (with wraparound support)
 * \param t1 First number
 * \param t2 Second number
 * \return  The  function  returns an integer less than, equal to, or greater than zero if the
 *   first number \p t1 is found, respectively, to be less than, to match, or be greater than
 *   the second number.
 */
static inline int
parser_seq_num_cmp(uint32_t t1, uint32_t t2)
{
    if (t1 == t2) {
        return 0;
    }

    if ((t1 - t2) & 0x80000000) { // test the "sign" bit
        return (-1);
    } else {
        return 1;
    }
}

/** Parser data used during processing an IPFIX message */
struct ipx_parser_data {
    /** Message Parser                                  */
    struct ipx_parser *parser;
    /** Wrapper over an IPFIX Message                   */
    struct ipx_msg_ipfix *ipfix_msg;
    /** Template manager                                */
    fds_tmgr_t *tmgr;

    /** Number of parser data records                   */
    uint16_t data_recs;
    /** Templates added/removed                         */
    bool tmplt_changes;
};

/**
 * \brief Process an (All) (Options) Template Withdrawal record
 *
 * Based on the record type, try to remove one or more (Options) Template
 * \param[in,out] pdata Parser internal data (Message context, Template manager, etc.)
 * \param[in]     rec   Withdrawal record
 * \param[in]     type  Type of enclosing IPFIX Set
 * \return #IPX_OK on success (all records successfully processed)
 * \return #IPX_ERR_FORMAT if an unexpected formatting error has been detected
 * \return #IPX_ERR_ARG in case of an internal error
 * \return #IPX_ERR_NOMEM if a memory allocation error has occurred
 */
static inline int
parser_parse_withdrawal(struct ipx_parser_data *pdata, struct fds_ipfix_wdrl_trec *rec,
    enum fds_template_type type)
{
    const struct ipx_msg_ctx *msg_ctx = &pdata->ipfix_msg->ctx;
    uint16_t tid = ntohs(rec->template_id);
    assert(ntohs(rec->count) == 0);

    PARSER_DEBUG(pdata->parser, msg_ctx, "Processing a request to withdraw an (Options) Template "
        "ID %" PRIu16 " ...", tid);

    if (pdata->ipfix_msg->ctx.session->type == FDS_SESSION_UDP) {
        // In case of UDP, ignore all requests
        PARSER_WARNING(pdata->parser, msg_ctx, "Ignoring an (Options) Template Withdrawal over UDP "
            "(Template ID %" PRIu16 ").", tid);
        return IPX_OK;
    }

    int rc;
    if (tid >= FDS_IPFIX_SET_MIN_DSET) {
        // (Options) Template Withdrawal
        rc = fds_tmgr_template_withdraw(pdata->tmgr, tid, type);
    } else if (tid == FDS_IPFIX_SET_TMPLT) {
        // All Templates Withdrawal
        assert(type == FDS_TYPE_TEMPLATE);
        rc = fds_tmgr_template_withdraw_all(pdata->tmgr, FDS_TYPE_TEMPLATE);
    } else if (tid == FDS_IPFIX_SET_OPTS_TMPLT) {
        // All Options Template Withdrawal
        assert(type == FDS_TYPE_TEMPLATE_OPTS);
        rc = fds_tmgr_template_withdraw_all(pdata->tmgr, FDS_TYPE_TEMPLATE_OPTS);
    } else {
        // Invalid Template ID
        rc = FDS_ERR_ARG;
    }

    if (rc == FDS_OK) {
        // Success
        if (tid >= FDS_IPFIX_SET_MIN_DSET) {
            PARSER_INFO(pdata->parser, msg_ctx, "A definition of the %s ID %" PRIu16 " has been "
                "withdrawn.", (type == FDS_TYPE_TEMPLATE) ? "Template" : "Options Template", tid);
        } else {
            PARSER_INFO(pdata->parser, msg_ctx, "Definitions of All %s have been withdrawn.",
                (type == FDS_TYPE_TEMPLATE) ? "Templates" : "Options Templates");
        }

        return IPX_OK;
    }

    // Something bad happened
    if (rc == FDS_ERR_ARG && tid >= FDS_IPFIX_SET_MIN_DSET) {
        // Template type mismatch - this is not a fatal error (try again)
        PARSER_WARNING(pdata->parser, msg_ctx, "Mismatch between template type to withdraw and "
            "type of template definition of Template ID %" PRIu16 " (Options Template vs Template "
            "or vice versa).", tid);

        rc = fds_tmgr_template_withdraw(pdata->tmgr, tid, FDS_TYPE_TEMPLATE_UNDEF);
        if (rc == FDS_OK) {
            PARSER_INFO(pdata->parser, msg_ctx, "A definition of the %s ID %" PRIu16 " has been "
                "withdrawn.", (type == FDS_TYPE_TEMPLATE) ? "Template" : "Options Template", tid);
            return IPX_OK;
        }
    }

    switch (rc) {
    case FDS_ERR_NOMEM:
        // Memory allocation failed
        PARSER_ERROR(pdata->parser, msg_ctx, "A memory allocation failed (%s:%d).",
            __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    case FDS_ERR_NOTFOUND:
        // Template to withdraw not found
        PARSER_WARNING(pdata->parser, msg_ctx, "Ignoring (Options) Template Withdrawal for "
            "undefined (Options) Template ID %" PRIu16 ".", tid);
        return IPX_OK;
    case FDS_ERR_DENIED:
        // Session type doesn't allow (Options) Template Withdrawal
        PARSER_WARNING(pdata->parser, msg_ctx, "(Options) Templates Withdrawals are prohibited "
            "over this type of Transport Session. Ignoring request to withdraw Template ID "
            "%" PRIu16 ".", tid);
        return IPX_OK;
    default:
        PARSER_ERROR(pdata->parser, msg_ctx, "fds_tmgr_template_withdraw*() returned an unexpected "
            "error code (%s:%d, code: %d).", __FILE__, __LINE__, rc);
        return IPX_ERR_ARG;
    }
}

/**
 * \brief Process an (Options) Template definition
 *
 * Parse a template definition and try to add it into a Template manager
 * \param[in,out] pdata Parser internal data (Message context, Template manager, etc.)
 * \param[in]     rec   Start of (Options) Template record (header)
 * \param[in]     type  Type of the template (FDS_TYPE_TEMPLATE or FDS_TYPE_TEMPLATE_OPTS)
 * \param[in]     size  Size of the template
 * \return #IPX_OK on success (all records successfully processed)
 * \return #IPX_ERR_FORMAT if an unexpected formatting error has been detected
 * \return #IPX_ERR_ARG in case of an internal error
 * \return #IPX_ERR_NOMEM if a memory allocation error has occurred
 */
static inline int
parser_parse_def(struct ipx_parser_data *pdata, void *rec, enum fds_template_type type,
    uint16_t size)
{
    int rc;
    const struct ipx_msg_ctx *msg_ctx = &pdata->ipfix_msg->ctx;
    uint16_t tid = ntohs(((struct fds_ipfix_trec *) rec)->template_id);
    struct fds_template *tmplt;

    PARSER_DEBUG(pdata->parser, msg_ctx, "Processing a definition of %s ID %" PRIu16 " ...",
        (type == FDS_TYPE_TEMPLATE) ? "Template" : "Options Template", tid);

    // Parse the (Options) Template
    if ((rc = fds_template_parse(type, rec, &size, &tmplt)) != FDS_OK) {
        // Something bad happened
        switch (rc) {
        case FDS_ERR_FORMAT:
            // Invalid definition
            PARSER_ERROR(pdata->parser, msg_ctx, "Invalid definition format of (Options) "
                "Template ID %" PRIu16 ".", tid);
            return IPX_ERR_FORMAT;
        case FDS_ERR_NOMEM:
            // Memory allocation failed
            PARSER_ERROR(pdata->parser, msg_ctx, "A memory allocation failed (%s:%d).",
                __FILE__, __LINE__);
            return IPX_ERR_NOMEM;
        default:
            // Unexpected situation
            PARSER_ERROR(pdata->parser, msg_ctx, "fds_template_parse() returned an unexpected "
                "error code (%s:%d, code: %d).", __FILE__, __LINE__, rc);
            return FDS_ERR_ARG;
        }
    }

    // Add (Options) Template
    if ((rc = fds_tmgr_template_add(pdata->tmgr, tmplt)) != FDS_OK) {
        // Something bad happened
        fds_template_destroy(tmplt);

        switch (rc) {
        case FDS_ERR_DENIED:
            // Template cannot be added due to protocol restriction (e.g. must be withdrawn first)
            PARSER_ERROR(pdata->parser, msg_ctx, "Unable to add (Options) Template ID %" PRIu16 " "
                "due to the IPFIX protocol restriction (e.g. must be withdrawn first, etc.).", tid);
            return IPX_ERR_FORMAT;
        case FDS_ERR_NOMEM:
            // Memory allocation failed
            PARSER_ERROR(pdata->parser, msg_ctx, "A memory allocation failed in a template manager "
                "(%s:%d).", __FILE__, __LINE__);
            return IPX_ERR_NOMEM;
        default:
            // Unexpected situation
            PARSER_ERROR(pdata->parser, msg_ctx, "fds_tmgr_template_add() returned an unexpected "
                "error code (%s:%d, code %d).", __FILE__, __LINE__, rc);
            return IPX_ERR_ARG;
        }
    }

    PARSER_INFO(pdata->parser, msg_ctx, "A definition of the %s ID %" PRIu16 " has been accepted.",
        (type == FDS_TYPE_TEMPLATE) ? "Template" : "Options Template", tid);

    return IPX_OK;
}

/**
 * \brief Parse (Options) Template Set
 *
 * New templates can be added or old templates can be withdrawn here.
 * \param[in,out] pdata Parser internal data (Message context, Template manager, etc.)
 * \param[in]     tset  Pointer to the Set header
 * \return #IPX_OK on success (all records successfully processed)
 * \return #IPX_ERR_FORMAT if an unexpected formatting error has been detected
 * \return #IPX_ERR_ARG in case of an internal error
 * \return #IPX_ERR_NOMEM if a memory allocation error has occurred
 */
static inline int
parser_parse_tset(struct ipx_parser_data *pdata, struct fds_ipfix_set_hdr *tset)
{
    // Processing templates
    pdata->tmplt_changes = true;

    uint16_t set_id = ntohs(tset->flowset_id);
    assert(set_id == FDS_IPFIX_SET_TMPLT || set_id == FDS_IPFIX_SET_OPTS_TMPLT);
    // Get type of the templates
    enum fds_template_type type = (set_id == FDS_IPFIX_SET_TMPLT)
        ? FDS_TYPE_TEMPLATE : FDS_TYPE_TEMPLATE_OPTS;

    // Process all definitions/withdrawals in the Set
    struct fds_tset_iter it;
    fds_tset_iter_init(&it, tset);

    int rc_iter;
    int rc_parse = IPX_OK;

    while (rc_parse == IPX_OK && (rc_iter = fds_tset_iter_next(&it)) == FDS_OK) {
        // Process the record
        if (it.field_cnt == 0) {
            // (All) (Options) Template Withdrawal
            rc_parse = parser_parse_withdrawal(pdata, it.ptr.wdrl_trec, type);
        } else if (it.scope_cnt > 0) {
            // Options Template definition
            rc_parse = parser_parse_def(pdata, it.ptr.opts_trec, FDS_TYPE_TEMPLATE_OPTS, it.size);
        } else {
            // Template definition
            rc_parse = parser_parse_def(pdata, it.ptr.trec, FDS_TYPE_TEMPLATE, it.size);
        }
    }

    if (rc_parse != IPX_OK) {
        // Proper error message has been already generated
        return rc_parse;
    }

    if (rc_iter != FDS_EOC) {
        const struct ipx_msg_ctx *msg_ctx = &pdata->ipfix_msg->ctx;
        PARSER_ERROR(pdata->parser, msg_ctx, "Failed to parse an IPFIX (Options) Template (%s).",
            fds_tset_iter_err(&it));
        return IPX_ERR_FORMAT;
    }

    return IPX_OK;
}

/**
 * \brief Parser Data Records in an IPFIX Set
 *
 * First, find an (Options) Template necessary to decode structure of records in this Set and
 * then detect the start position of each Data Record and mark it.
 * \param[in,out] pdata Parser internal data (Message context, Template manager, etc.)
 * \param[in]     dset  Pointer to the Set header
 * \return #IPX_OK on success (all records successfully processed)
 * \return #IPX_ERR_FORMAT if an unexpected formatting error has been detected
 * \return #IPX_ERR_ARG in case of an internal error
 * \return #IPX_ERR_NOMEM if a memory allocation error has occurred
 */
static inline int
parser_parse_dset(struct ipx_parser_data *pdata, struct fds_ipfix_set_hdr *dset)
{
    uint16_t set_id = ntohs(dset->flowset_id);
    assert(set_id >= FDS_IPFIX_SET_MIN_DSET);

    // Find a Snapshot
    int rc;
    const fds_tsnapshot_t *snap;
    if ((rc = fds_tmgr_snapshot_get(pdata->tmgr, &snap)) != FDS_OK) {
        // Something bad happened
        const struct ipx_msg_ctx *msg_ctx = &pdata->ipfix_msg->ctx;
        if (rc == FDS_ERR_NOMEM) {
            // Memory allocation failure
            PARSER_ERROR(pdata->parser, msg_ctx, "A memory allocation failed (%s:%d).",
                __FILE__, __LINE__);
            return IPX_ERR_NOMEM;
        } else {
            // Unexpected error
            PARSER_ERROR(pdata->parser, msg_ctx, "fds_tmgr_snapshot_get() returned an unexpected "
                "error code (%s:%d, code: %d).", __FILE__, __LINE__, rc);
            return IPX_ERR_ARG;
        }
    }

    // Find an (Options) Template
    const struct fds_template *tmplt = fds_tsnapshot_template_get(snap, set_id);
    if (!tmplt) {
        const struct ipx_msg_ctx *msg_ctx = &pdata->ipfix_msg->ctx;
        PARSER_WARNING(pdata->parser, msg_ctx, "Unable to parse IPFIX Data Set %" PRIu16 " "
            "due to missing (Options) Template.", set_id);
        return IPX_OK;
    }

    struct fds_drec rec;
    rec.tmplt = tmplt;
    rec.snap = snap;

    // Parse Data Records in the Set
    struct fds_dset_iter it;
    fds_dset_iter_init(&it, dset, tmplt);

    while ((rc = fds_dset_iter_next(&it)) == FDS_OK) {
        // Add a new record
        rec.data = it.rec;
        rec.size = it.size;

        struct ipx_ipfix_record *added_ref = ipx_msg_ipfix_add_drec_ref(&pdata->ipfix_msg);
        if (!added_ref) {
            const struct ipx_msg_ctx *msg_ctx = &pdata->ipfix_msg->ctx;
            PARSER_ERROR(pdata->parser, msg_ctx, "Memory allocation failed (%s:%d).",
                __FILE__, __LINE__);
            return IPX_ERR_NOMEM;
        }

        // Store the reference
        added_ref->rec = rec;
        pdata->data_recs++;
    }

    if (rc == FDS_EOC) {
        return IPX_OK;
    }

    // Malformed data structure
    assert(rc == FDS_ERR_FORMAT);
    const struct ipx_msg_ctx *msg_ctx = &pdata->ipfix_msg->ctx;
    PARSER_ERROR(pdata->parser, msg_ctx, "Failed to process a Data Record in a Data Set ID "
        "%" PRIu16 " (%s).", set_id, fds_dset_iter_err(&it));

    // Try to remove the Template definition
    rc = fds_tmgr_template_remove(pdata->tmgr, set_id, FDS_TYPE_TEMPLATE_UNDEF);
    switch (rc) {
    case FDS_OK:
        // The template has been successfully removed
        PARSER_WARNING(pdata->parser, msg_ctx, "Template ID %" PRIu16 " has been removed due to "
            "the previous processing failure.", set_id);
        return IPX_ERR_FORMAT;
    case FDS_ERR_NOMEM:
        // Memory allocation failure
        PARSER_ERROR(pdata->parser, msg_ctx, "A memory allocation failed (%s:%d).",
            __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    default:
        // Unexpected error
        PARSER_ERROR(pdata->parser, msg_ctx, "fds_tmgr_template_remove() returned an unexpected "
            "error code (%s:%d, code: %d).", __FILE__, __LINE__, rc);
        return IPX_ERR_ARG;
    }
}

/**
 * \brief Parse IPFIX Message
 *
 * Iterate over each IPFIX Set and process records. Based on content of the Message, (Options)
 * Templates can be added/removed to the Template manager. Positions of Data records in the Message
 * and references to their Templates will be marked in the wrapper.
 *
 * \param[in,out] pdata Parser internal data (Message context, Template manager, etc.)
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT if a formatting error has been detected and the messages MUST be
 *   destroyed by the user
 * \return #IPX_ERR_NOMEM if a memory allocation error has occurred. User should close the
 *   Transport Session, if possible.
 * \return #IPX_ERR_ARG if an internal error has occurred.
 */
static inline int
parser_parse_message(struct ipx_parser_data *pdata)
{
    int rc_iter;
    int rc_parse = IPX_OK;

    struct fds_sets_iter it;
    fds_sets_iter_init(&it, (struct fds_ipfix_msg_hdr *) pdata->ipfix_msg->raw_pkt);

    // Iterate over all Sets in the IPFIX Message
    while (rc_parse == IPX_OK && (rc_iter = fds_sets_iter_next(&it)) == FDS_OK) {
        uint16_t set_id = ntohs(it.set->flowset_id);

        if (set_id >= FDS_IPFIX_SET_MIN_DSET) {
            // Data Set
            rc_parse = parser_parse_dset(pdata, it.set);
        } else if (set_id == FDS_IPFIX_SET_TMPLT || set_id == FDS_IPFIX_SET_OPTS_TMPLT) {
            // (Options) Template Set
            rc_parse = parser_parse_tset(pdata, it.set);
        } else {
            // Unknown Set ID
            const struct ipx_msg_ctx *msg_ctx = &pdata->ipfix_msg->ctx;
            PARSER_WARNING(pdata->parser, msg_ctx, "Skipping unknown Set ID %" PRIu16 ".", set_id);
            rc_parse = IPX_OK;
        }

        // Add a reference to the Set
        struct ipx_ipfix_set *set_ref = ipx_msg_ipfix_add_set_ref(pdata->ipfix_msg);
        if (!set_ref) {
            // Memory allocation failure
            const struct ipx_msg_ctx *msg_ctx = &pdata->ipfix_msg->ctx;
            PARSER_ERROR(pdata->parser, msg_ctx, "A memory allocation failed (%s:%d).",
                __FILE__, __LINE__);
            return IPX_ERR_NOMEM;
        }

        set_ref->ptr = it.set;
    }

    if (rc_parse != IPX_OK) {
        // Proper error message has been already generated
        return rc_parse;
    }

    if (rc_iter != FDS_EOC) {
        const struct ipx_msg_ctx *msg_ctx = &pdata->ipfix_msg->ctx;
        PARSER_ERROR(pdata->parser, msg_ctx, "Failed to parse an IPFIX Set (%s).",
            fds_sets_iter_err(&it));
        return IPX_ERR_FORMAT;
    }

    return IPX_OK;
}

/**
 * \brief Set block flag to all Transport Session and ODIDs in the parser
 *
 * After calling this function, the parser rejects to process particular IPFIX Messages of a TS
 * until the TS is removed by ipx_parser_session_remove()
 * \param parser Parser
 */
static inline void
parser_session_block_all(ipx_parser_t *parser)
{
    for (size_t idx = 0; idx < parser->recs_valid; ++idx) {
        struct stream_ctx *ctx = parser->recs[idx].ctx;
        ctx->flags |= SCF_BLOCK;
    }
}

/**
 * \brief Convert a message with flow records to IPFIX Message format
 *
 * If the message is already in IPFIX Message format, no conversion is performed.
 * The function also check if the message type matches previously seen type of messages
 * in the stream.
 *
 * @param[in]     parser Parser (for logging information)
 * @param[in]     rec    Parser record of the current stream
 * @param[in,out] msg    Message wrapper with a message to convert
 *
 * @return #IPX_OK on success
 * @return #IPX_ERR_DENIED if the message cannot be converted (i.e. not NetFlow or IPFIX)
 * @return #IPX_ERR_FORMAT if the message is malformed and stream must be closed/reset
 * @return #IPX_ERR_NOMEM in case of a memory allocation error
 */
static int
parser_convert(ipx_parser_t *parser, struct parser_rec *rec, ipx_msg_ipfix_t *msg)
{
    const struct ipx_msg_ctx *msg_ctx = &msg->ctx;
    const uint8_t *msg_data = msg->raw_pkt;
    const uint16_t msg_size = msg->raw_size;

    if (msg_size < sizeof(uint16_t)) {
        return FDS_ERR_FORMAT;
    }

    // Determine the version of flow message
    const uint16_t version = ntohs(*(uint16_t *) msg_data);

    enum source_type type = rec->ctx->type;
    if (type == ST_UNKNOWN) {
        // This is the first message that we received for processing
        switch (version) {
        case FDS_IPFIX_VERSION:
            // IPFIX
            rec->ctx->type = ST_IPFIX;
            break;
        case IPX_NF9_VERSION:
            // NetFlow v9 (+ initialize converter)
            rec->ctx->type = ST_NETFLOW9;
            rec->ctx->converter.nf9 = ipx_nf9_conv_init(parser->ident, parser->vlevel);
            if (!rec->ctx->converter.nf9) {
                PARSER_ERROR(parser, msg_ctx, "Failed to initialize NetFlow v9 converter!", '\0');
                return IPX_ERR_NOMEM;
            }
            break;
        case IPX_NF5_VERSION: {
            // NetFlow v5 (+ initialize converter)
            rec->ctx->type = ST_NETFLOW5;

            // Determine suitable Template refresh interval
            uint32_t tmplt_refresh = 0; // disabled
            if (rec->session->type == FDS_SESSION_UDP) {
                // Lifetime should be at least 3x higher than refresh interval
                tmplt_refresh = rec->session->udp.lifetime.tmplts / 3U;
            }

            rec->ctx->converter.nf5 = ipx_nf5_conv_init(parser->ident, parser->vlevel,
                tmplt_refresh, rec->odid);
            if (!rec->ctx->converter.nf5) {
                PARSER_ERROR(parser, msg_ctx, "Failed to initialize NetFlow v5 converter!", '\0');
                return IPX_ERR_NOMEM;
            }
            }
            break;
        default:
            PARSER_ERROR(parser, msg_ctx, "Unexpected NetFlow/IPFIX message version (expected: "
                "5,9 or 10, got: %" PRIu16 ")", version);
            return IPX_ERR_DENIED;
        }
    }

    type = rec->ctx->type;
    assert(type != ST_UNKNOWN);

    // Perform convertion based on the stream type, if necessary
    int conv_status = IPX_OK;
    switch (type) {
    case ST_IPFIX:
        // IPFIX
        if (version != FDS_IPFIX_VERSION) {
            PARSER_ERROR(parser, msg_ctx, "Expected an IPFIX Message but non-IPFIX data has been "
                "received (expected version: 10, got: %" PRIu16 ")", version);
            return IPX_ERR_FORMAT;
        }

        // Nothing to convert...
        break;
    case ST_NETFLOW9:
        // NetFlow v9 -> convert to IPFIX
        if (version != IPX_NF9_VERSION) {
            PARSER_ERROR(parser, msg_ctx, "Expected NetFlow v9 Message but non-NetFlow data has "
                "been received (expected version: 9, got: %" PRIu16 ")", version);
            return IPX_ERR_FORMAT;
        }

        conv_status = ipx_nf9_conv_process(rec->ctx->converter.nf9, msg);
        break;
    case ST_NETFLOW5:
        // NetFlow v5 -> convert to IPFIX
        if (version != IPX_NF5_VERSION) {
            PARSER_ERROR(parser, msg_ctx, "Expected NetFlow v5 Message but non-NetFlow data has "
                "been received (expected version: 5, got: %" PRIu16 ")", version);
            return IPX_ERR_FORMAT;
        }

        conv_status = ipx_nf5_conv_process(rec->ctx->converter.nf5, msg);
        break;
    default:
        PARSER_ERROR(parser, msg_ctx, "Unimplemented support for message format conversion!", '\0');
        return IPX_ERR_DENIED;
    }

    if (conv_status != IPX_OK) {
        return (conv_status == IPX_ERR_NOMEM) ? IPX_ERR_NOMEM : IPX_ERR_FORMAT;
    }

    return IPX_OK;
}

ipx_parser_t *
ipx_parser_create(const char *ident, enum ipx_verb_level vlevel)
{
    struct ipx_parser *parser = calloc(1, sizeof(*parser));
    if (!parser) {
        return NULL;
    }

    const size_t alloc_size = PARSER_DEF_RECS * sizeof(*parser->recs);
    parser->recs = malloc(alloc_size);
    if (!parser->recs) {
        free(parser);
        return NULL;
    }

    parser->ident = strdup(ident);
    if (!parser->ident) {
        free(parser->recs);
        free(parser);
        return NULL;
    }

    parser->vlevel = vlevel;
    parser->recs_alloc = PARSER_DEF_RECS;
    parser->ie_mgr = NULL;
    return parser;
}

void
ipx_parser_destroy(ipx_parser_t *parser)
{
    // Destroy all stream contexts
    for (size_t idx = 0; idx < parser->recs_valid; ++idx) {
        stream_ctx_destroy(parser->recs[idx].ctx);
    }

    free(parser->ident);
    free(parser->recs);
    free(parser);
}

void
ipx_parser_verb(ipx_parser_t *parser, enum ipx_verb_level *v_new, enum ipx_verb_level *v_old)
{
    if (v_old != NULL) {
        *v_old = parser->vlevel;
    }

    if (v_new != NULL) {
        parser->vlevel = *v_new;

        // Change verbosity of all converters too
        for (size_t i = 0; i < parser->recs_valid; ++i) {
            struct stream_ctx *ctx = parser->recs[i].ctx;

            if (ctx->type == ST_NETFLOW5 && ctx->converter.nf5 != NULL) {
                ipx_nf5_conv_verb(ctx->converter.nf5, *v_new);
            }
            if (ctx->type == ST_NETFLOW9 && ctx->converter.nf9 != NULL) {
                ipx_nf9_conv_verb(ctx->converter.nf9, *v_new);
            }
        }
    }
}

int
ipx_parser_process(ipx_parser_t *parser, ipx_msg_ipfix_t **ipfix, ipx_msg_garbage_t **garbage)
{
    *garbage = NULL;
    const struct ipx_msg_ctx *msg_ctx = &(*ipfix)->ctx;

    // Find a Stream Info
    struct parser_rec *rec;   // Combination of Transport Session, ODID
    struct stream_info *info; // Combination of Transport Session, ODID and Stream ID
    if ((rec = parser_rec_get(parser, msg_ctx)) == NULL) {
        PARSER_ERROR(parser, msg_ctx, "A memory allocation failed (%s:%d).", __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }

    if ((rec->ctx->flags & SCF_BLOCK) != 0) {
        // This Transport Session has been blocked due to previous invalid behaviour
        return IPX_ERR_DENIED;
    }

    if ((info = stream_ctx_rec_get(&rec->ctx, msg_ctx->stream)) == NULL) {
        PARSER_ERROR(parser, msg_ctx, "A memory allocation failed (%s:%d).", __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }
    assert(rec->session == msg_ctx->session);
    assert(rec->odid == msg_ctx->odid);
    assert(info->id == msg_ctx->stream);

    // Check if the message must be converted to IPFIX
    int conv_status = parser_convert(parser, rec, *ipfix);
    if (conv_status != IPX_OK) {
        // Note: An appropriate error message has been printed in the converter
        if (conv_status != FDS_ERR_NOMEM) {
            return FDS_ERR_FORMAT;
        }

        return FDS_ERR_NOMEM;
    }

    // Check IPFIX Message header and its sequence number
    const struct fds_ipfix_msg_hdr *msg_data = (struct fds_ipfix_msg_hdr *)(*ipfix)->raw_pkt;
    assert(ntohs(msg_data->version) == FDS_IPFIX_VERSION && "Message to parse must be IPFIX!");

    if (ntohs(msg_data->length) < FDS_IPFIX_MSG_HDR_LEN) {
        PARSER_ERROR(parser, msg_ctx, "IPFIX Message Header size (%" PRIu16 ") is invalid "
            "(total length of the message is smaller than the IPFIX Message Header structure).",
            ntohs(msg_data->length));
        return IPX_ERR_FORMAT;
    }

    // Sequence number check
    uint32_t msg_seq = ntohl(msg_data->seq_num);
    PARSER_DEBUG(parser, msg_ctx, "Processing an IPFIX Message (Seq. number %" PRIu32 ")",
        msg_seq);

    bool old_oos = false;  // Old out of sequence message
    if (info->seq_num != msg_seq) {
        if ((info->flags & SIF_SEEN) == 0) {
            // The first message from this combination of the TS, ODID and Stream ID
            info->flags |= SIF_SEEN;
            info->seq_num = msg_seq;
        } else {
            // Out of sequence message
            PARSER_WARNING(parser, msg_ctx, "Unexpected Sequence number (expected: "
                "%" PRIu32 ", got: %" PRIu32 ").", info->seq_num, msg_seq);
            if (parser_seq_num_cmp(msg_seq, info->seq_num) > 0) {
                info->seq_num = msg_seq; // Newer than expected
            } else {
                old_oos = true; // Older than expected
            }
        }
    }
    info->flags |= SIF_SEEN;

    // Configure a template manager
    fds_tmgr_t *tmgr = rec->ctx->mgr;
    int rc;
    if ((rc = fds_tmgr_set_time(tmgr, ntohl(msg_data->export_time))) != FDS_OK) {
        switch (rc) {
        case FDS_ERR_DENIED:
            // Failed to set Export Time
            PARSER_ERROR(parser, msg_ctx, "Setting Export Time in history is not allowed for "
                "this type of Transport Session.", 0);
            return IPX_ERR_FORMAT;
        case FDS_ERR_NOTFOUND:
            // Unable to find a corresponding snapshot
            PARSER_WARNING(parser, msg_ctx, "Received IPFIX Message has too old Export Time. "
                "Templates are no longer available and therefore, all its data records are "
                "ignored.", 0);
            return IPX_OK;
        case FDS_ERR_NOMEM:
            // Memory allocation failed
            PARSER_ERROR(parser, msg_ctx, "A memory allocation failed (%s:%d).",
                __FILE__, __LINE__);
            return IPX_ERR_NOMEM;
        default:
            // Unexpected error
            PARSER_ERROR(parser, msg_ctx, "fds_tmgr_set_time() returned an unexpected error "
                "code (%s:%d, code: %d).", __FILE__, __LINE__, rc);
            return IPX_ERR_ARG;
        }
    }

    // Parse IPFIX Sets
    struct ipx_parser_data parser_data = {
        .parser = parser,
        .ipfix_msg = *ipfix,
        .tmgr = tmgr,
        .data_recs = 0,
        .tmplt_changes = false
    };
    rc = parser_parse_message(&parser_data);

    /* Warning: do NOT use "msg_ctx" because IPFIX message MAY be reallocated and the pointer
     * is NOT valid anymore! If you need it, obtain it again.
     *
     * Because the wrapper could be reallocated, update the pointer...
     */
    *ipfix = parser_data.ipfix_msg;
    if (rc != IPX_OK) {
        // Hide internal message as memory allocation error
        if (rc == IPX_ERR_ARG) {
            rc = IPX_ERR_NOMEM;
        }

        return rc;
    }

    // Update expected Sequence number of the next message
    if (!old_oos) {
        info->seq_num += parser_data.data_recs;
    }

    ipx_msg_garbage_t *garbage_msg = NULL;
    if (parser_data.tmplt_changes) {
        // There is potentially garbage to destroy
        fds_tgarbage_t *fds_garbage;
        if (fds_tmgr_garbage_get(tmgr, &fds_garbage) == FDS_OK && fds_garbage != NULL) {
            ipx_msg_garbage_cb cb = (ipx_msg_garbage_cb) &fds_tmgr_garbage_destroy;
            garbage_msg = ipx_msg_garbage_create(fds_garbage, cb);
        }
    }

    *garbage = garbage_msg;
    return IPX_OK;
}

int
ipx_parser_ie_source(ipx_parser_t *parser, const fds_iemgr_t *iemgr, ipx_msg_garbage_t **garbage)
{
    parser->ie_mgr = iemgr;

    // First, try to update all Template managers
    size_t idx;
    for (idx = 0; idx < parser->recs_valid; idx++) {
        // Skip disabled sources
        struct stream_ctx *ctx = parser->recs[idx].ctx;
        if ((ctx->flags & SCF_BLOCK) != 0) {
            continue;
        }

        if (fds_tmgr_set_iemgr(ctx->mgr, iemgr) == FDS_OK) {
            continue;
        }

        // Memory allocation failed -> we cannot continue
        IPX_ERROR(parser->ident, "fds_tmgr_set_iemgr() failed to replace old IE definitions in "
            "a template manager due to a memory allocation error (%s:%d).", __FILE__, __LINE__);
        parser_session_block_all(parser);
        return IPX_ERR_NOMEM;
    }

    // Prepare to collect garbage
    ipx_gc_t *gc = NULL;
    if ((gc = ipx_gc_create()) == NULL || ipx_gc_reserve(gc, parser->recs_valid) != IPX_OK) {
        // Failed to create a garbage container
        ipx_gc_destroy(gc);
        parser_session_block_all(parser);
        return IPX_ERR_NOMEM;
    }

    // Clean up
    for (idx = 0; idx < parser->recs_valid; idx++) {
        // Get old templates and snapshots as garbage
        struct stream_ctx *ctx = parser->recs[idx].ctx;
        fds_tgarbage_t *fds_garbage;

        if (fds_tmgr_garbage_get(ctx->mgr, &fds_garbage) != FDS_OK) {
            // Garbage lost (memory leak)
            IPX_ERROR(parser->ident, "A memory allocation failed (%s:%d).", __FILE__, __LINE__);
            continue;
        }

        ipx_msg_garbage_cb cb = (ipx_msg_garbage_cb) &fds_tmgr_garbage_destroy;
        if (ipx_gc_add(gc, fds_garbage, cb) != IPX_OK) {
            // Garbage lost (memory leak)
            IPX_ERROR(parser->ident, "ipx_gc_add() failed! (%s:%d).", __FILE__, __LINE__);
        }
    }

    // Success
    if (ipx_gc_empty(gc)) {
        *garbage = NULL;
        ipx_gc_destroy(gc);
        return IPX_OK;
    }

    ipx_msg_garbage_t *msg = ipx_gc_to_msg(gc);
    if (!msg) {
        // We cannot free garbage, someone is potentially using templates inside! (memory leak)
        *garbage = NULL;
        IPX_ERROR(parser->ident, "A memory allocation failed (%s:%d).", __FILE__, __LINE__);
        return IPX_OK;
    }

    *garbage = msg;
    return IPX_OK;
}

int
ipx_parser_session_remove(ipx_parser_t *parser, const struct ipx_session *session,
    ipx_msg_garbage_t **garbage)
{
    size_t idx_start; // Index of the first occurrence of the session records
    size_t idx_end;   // Index of the "past-the-last" occurrence of the session records

    // Find records to remove (the array is primary sorted by Transport Session)
    for (idx_start = 0; idx_start < parser->recs_valid; ++idx_start) {
        if (parser->recs[idx_start].session == session) {
            break;
        }
    }

    if (idx_start == parser->recs_valid) {
        // Not found
        return IPX_ERR_NOTFOUND;
    }

    for (idx_end = idx_start + 1; idx_end < parser->recs_valid; ++idx_end) {
        if (parser->recs[idx_end].session != session) {
            break;
        }
    }

    // Move session data into garbage
    ipx_msg_garbage_t *garbage_msg = parser_rec_to_garbage(parser, idx_start, idx_end);
    /* Note: If the garbage message is NULL, allocation of the memory failed and information about
     * session will be lost. We cannot free structures here because someone still could use them.
     * (This will cause a memory leak but its better that segfault!)
     */

    // Remove old records (order will remain still the same -> sort is not necessary)
    while (idx_end < parser->recs_valid) {
        parser->recs[idx_start++] = parser->recs[idx_end++];
    }

    // Update number of valid records
    parser->recs_valid = idx_start;
    *garbage = garbage_msg;
    return IPX_OK;
}

int
ipx_parser_session_block(ipx_parser_t *parser, const struct ipx_session *session)
{
    size_t idx_start; // Index of the first occurrence of the session records

    // Find records to remove (the array is primary sorted by Transport Session)
    for (idx_start = 0; idx_start < parser->recs_valid; ++idx_start) {
        if (parser->recs[idx_start].session == session) {
            break;
        }
    }

    if (idx_start == parser->recs_valid) {
        // Not found
        return IPX_ERR_NOTFOUND;
    }

    for (size_t idx = idx_start; idx < parser->recs_valid; ++idx) {
        struct parser_rec *rec = &parser->recs[idx];
        if (rec->session != session) {
            // Stop, found a different session
            break;
        }

        // Set "block" flag
        parser->recs[idx].ctx->flags |= SCF_BLOCK;
    }

    return IPX_OK;
}

void
ipx_parser_session_for(ipx_parser_t *parser, ipx_parser_for_cb cb, void *data)
{
    /* Keep on mind that ipx_parser_session_block() and ipx_parser_session_remove() can be
     * called within the callback function i.e. records can be removed from the parser during for
     * loop!
     */
    const struct ipx_session *now = NULL;
    size_t idx = 0;
    while (idx < parser->recs_valid) {
        struct parser_rec *rec = &parser->recs[idx];
        if (rec->session == now) {
            // Skip already processed sessions
            idx++;
            continue;
        }

        now = rec->session;
        cb(parser, now, data); // Number of valid records can be changed here!
    }
}
