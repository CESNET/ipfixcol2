/**
 * \file src/core/parser.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief IPFIX Message parser (source file)
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

#include <stdlib.h>
#include <inttypes.h>
#include <libfds.h>
#include <ipfixcol2.h>

#include "message_ipfix.h"
#include "plugin.h"
#include "parser.h"

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
    /** Sequence number of the stream                                  */
    uint32_t seq_num;
};

/** Auxiliary flags specific to each Stream context                    */
enum stream_ctx_flags {
    /** Ignore all IPFIX messages                                      */
    SCF_BLOCK = (1 << 0),
    /** Close request of the Transport Session is available            */
    SCF_TS_CREQ = (1 << 1)
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

/** Main structure of IPFIX message parser        */
struct ipx_parser {
    /** Plugin context                            */
    struct ipx_ctx *plugin_ctx;

    /** Number of pre-allocated records           */
    size_t recs_alloc;
    /** Number of valid records                   */
    size_t recs_valid;
    /** Array of records                          */
    struct parser_rec *recs;
};

/**
 * \def PARSER_ERROR
 * \brief Macro for printing an error message of a parser
 * \param[in] ctx     Plugin context
 * \param[in] msg_ctx IPFIX Message context (Transport Session, ODID, ...)
 * \param[in] fmt     Format string (see manual page for "printf" family)
 * \param[in] ...     Variable number of arguments for the format string
 */
#define PARSER_ERROR(ctx, msg_ctx, fmt, ...) \
    IPX_ERROR((ctx), "[%s, ODID: %" PRIu32 "] " fmt, \
        (msg_ctx)->session->ident, (msg_ctx)->odid, ## __VA_ARGS__);
/**
 * \def PARSER_WARNING
 * \brief Macro for printing a warning message of a parser
 * \param[in] ctx     Plugin context
 * \param[in] msg_ctx IPFIX Message context (Transport Session, ODID, ...)
 * \param[in] fmt     Format string (see manual page for "printf" family)
 * \param[in] ...     Variable number of arguments for the format string
 */
#define PARSER_WARNING(ctx, msg_ctx, fmt, ...) \
    IPX_WARNING((ctx), "[%s, ODID: %" PRIu32 "] " fmt, \
        (msg_ctx)->session->ident, (msg_ctx)->odid, ## __VA_ARGS__);
/**
 * \def PARSER_INFO
 * \brief Macro for printing an info message of a parser
 * \param[in] ctx     Plugin context
 * \param[in] msg_ctx IPFIX Message context (Transport Session, ODID, ...)
 * \param[in] fmt     Format string (see manual page for "printf" family)
 * \param[in] ...     Variable number of arguments for the format string
 */
#define PARSER_INFO(ctx, msg_ctx, fmt, ...) \
    IPX_INFO((ctx), "[%s, ODID: %" PRIu32 "] " fmt, \
        (msg_ctx)->session->ident, (msg_ctx)->odid, ## __VA_ARGS__);
/**
 * \def PARSER_DEBUG
 * \brief Macro for printing a debug message of a parser
 * \param[in] ctx     Plugin context
 * \param[in] msg_ctx IPFIX Message context (Transport Session, ODID, ...)
 * \param[in] fmt     Format string (see manual page for "printf" family)
 * \param[in] ...     Variable number of arguments for the format string
 */
#define PARSER_DEBUG(ctx, msg_ctx, fmt, ...) \
    IPX_DEBUG((ctx), "[%s, ODID: %" PRIu32 "] " fmt, \
        (msg_ctx)->session->ident, (msg_ctx)->odid, ## __VA_ARGS__);

/**
 * \def STREAM_CTX_SIZE
 * \brief Get size of stream_ctx structure suitable for \p recs records
 */
#define STREAM_CTX_SIZE(recs)    \
    sizeof(struct stream_ctx)    \
    - sizeof(struct stream_info) \
    + (recs * sizeof(struct stream_info))

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

    const fds_iemgr_t *iemgr = parser->plugin_ctx->cfg_system.ie_mgr;
    if (fds_tmgr_set_iemgr(ctx->mgr, iemgr) != FDS_OK) {
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
 * \brief Find a parser record defined by Transport Session hash and Observation Domain ID within
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
 * \brief Get a parser record defined by Transport Session hash and Observation Domain ID within
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

    parser->recs_valid++;
    parser_rec_sort(parser);
    // Position has been changes... find it again
    rec = parser_rec_find(parser, ctx);
    assert(rec != NULL);
    return rec;
}

ipx_parser_t *
ipx_parser_create(ipx_ctx_t *plugin_ctx)
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

    // TODO: subscribe to modifications

    parser->plugin_ctx = plugin_ctx;
    parser->recs_alloc = PARSER_DEF_RECS;
    return parser;
}

void
ipx_parser_destroy(ipx_parser_t *parser)
{
    // TODO: unsubscribe to modifications

    // Destroy all stream contexts
    for (size_t idx = 0; idx < parser->recs_valid; ++idx) {
        stream_ctx_destroy(parser->recs[idx].ctx);
    }

    free(parser->recs);
    free(parser);
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
void session_garbage_destroy(struct session_gabage *grb)
{
    if (grb == NULL || grb->recs == NULL) {
        return;
    }

    for (size_t i = 0; i < grb->rec_cnt; ++i) {
        stream_ctx_destroy(grb->recs[i]);
    }
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
static struct session_gabage *
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

int
ipx_parser_session_remove(ipx_parser_t *parser, const struct ipx_session *session)
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
    ipx_msg_garbage_t *garbage = parser_rec_to_garbage(parser, idx_start, idx_end);
    /* Note: If the garbage message is NULL, allocation of the memory failed and information about
     * session will be lost. We cannot free structures here because someone still could use them.
     * (This will cause a memory leak but its better that segfault!)
     */
    if (garbage != NULL) {
        // TODO: send garbage message
    }

    // Remove old records (order will remain still the same -> sort is not necessary)
    while (idx_end < parser->recs_valid) {
        parser->recs[idx_start++] = parser->recs[idx_end++];
    }

    // Update number of valid records
    parser->recs_valid = idx_start;
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











struct parser_data {
    struct ipx_msg_ipfix *ipfix_msg;    /**< Wrapper over an IPFIX Message              */
    const ipx_ctx_t *plugin_ctx;        /**< Plugin context                             */
    fds_tmgr_t *tmgr;                   /**< Template manager                           */
};

static int
process_tset(struct fds_ipfix_set_hdr *set, struct parser_data *data)
{
    struct fds_tset_iter it;
    fds_tset_iter_init(&it, set);

    int rc_iter;
    int rc_parse = 0;

    while (rc_parse == 0 && (rc_iter = fds_tset_iter_next(&it)) == FDS_OK) {
        if (it.field_cnt == 0) {
            rc_parse = process_withdrawal(it.ptr.wdrl_trec);
        } else if (it.scope_cnt > 0) {
            rc_parse = process_def_opts(it.ptr.opts_trec, it.size);
        } else {
            rc_parse = process_def_data(it.ptr.trec, it.size);
        }
    }

    if (rc_parse != 0) {
        return rc_parse;
    }

    if (rc_iter != FDS_ERR_NOTFOUND) {
        const struct ipx_ctx *plugin_ctx = data->plugin_ctx;
        const struct ipx_msg_ctx *msg_ctx = &data->ipfix_msg->ctx;
        PARSER_ERROR(plugin_ctx, msg_ctx, "Failed to parse an IPFIX (Options) Template (%s).",
            fds_sets_iter_err(&it));
        return IPX_ERR_FORMAT;
    }

    return IPX_OK;
}

static int
process_dset(struct fds_ipfix_set_hdr *set, struct parser_data *data)
{
    uint16_t set_id = ntohs(set->flowset_id);
    const struct fds_template *tmplt;

    int rc = fds_tmgr_template_get(data->tmgr, set_id, &tmplt);
    // TODO:


    if ( != FDS_OK) {

        std::cerr << "Unable to find required template. Skipping..." << std::endl;
        return 0;
    }

    int rc;
    struct fds_drec rec;
    rec.tmplt = tmplt;
    rec.snap = NULL; // not supported yet

    struct fds_dset_iter it;
    fds_dset_iter_init(&it, set, tmplt);

    while ((rc = fds_dset_iter_next(&it)) == FDS_OK) {
        rec.data = it.rec;
        rec.size = it.size;
        if (process_rec(&rec) != 0) {
            return 1;
        }
    }

    if (rc != FDS_ERR_NOTFOUND) {
        fprintf(stderr, "Error: %s\n", fds_dset_iter_err(&it));
    }


    return IPX_OK;
}



static int
process_msg(struct parser_data *data)
{
    int rc_iter;
    int rc_parse = 0;
    struct fds_sets_iter it;
    fds_sets_iter_init(&it, data->ipfix_msg->raw_pkt);

    while (rc_parse == 0 && (rc_iter = fds_sets_iter_next(&it)) == FDS_OK) {
        uint16_t id = ntohs(it.set->flowset_id);
        if (id == FDS_IPFIX_SET_TMPLT || id == FDS_IPFIX_SET_OPTS_TMPLT) {
            // (Options) Template Set
            rc_parse = process_tset(it.set, data);
        } else if (id >= FDS_IPFIX_SET_MIN_DSET) {
            // Data Set
            rc_parse = process_dset(it.set, data);
        } else {
            // Unknown Set ID
            const struct ipx_ctx *plugin_ctx = data->plugin_ctx;
            const struct ipx_msg_ctx *msg_ctx = &data->ipfix_msg->ctx;
            PARSER_WARNING(plugin_ctx, msg_ctx, "Skipping unknown Set ID.");
            rc_parse = 0;
        }
    }

    if (rc_parse != 0) {
        return rc_parse;
    }

    if (rc_iter != FDS_ERR_NOTFOUND) {
        const struct ipx_ctx *plugin_ctx = data->plugin_ctx;
        const struct ipx_msg_ctx *msg_ctx = &data->ipfix_msg->ctx;
        PARSER_ERROR(plugin_ctx, msg_ctx, "Failed to parse an IPFIX Set (%s).",
            fds_sets_iter_err(&it));
        return IPX_ERR_FORMAT;
    }

    return IPX_OK;
}



/*
 * - odlišit seznam streamů - tak aby si tam kazdy mohl ukladat asi cokoliv...
 *
 * Co bude vracet:
 * - OK - vsechno (nebo nekriticke) se povedlo a zprava je privana k predani
 * - FMT - špatne formatovani zpravy... zprava by mela byt zahozena a spojeni by melo byt
 *         ukonceno
 *       - vlakno si nastavi, ze bude ignorovat vsechny jeho zpravy.. pokud to ma smysl...
 *
 *
 *
 *
 */

int
ipx_parser_process(ipx_parser_t *parser, ipx_msg_ipfix_t *ipfix_msg)
{
    int rc;
    const struct ipx_ctx *plugin_ctx = parser->plugin_ctx;
    struct fds_ipfix_msg_hdr *msg_data = ipfix_msg->raw_pkt;
    const struct ipx_msg_ctx *msg_ctx = &ipfix_msg->ctx;

    // Find a Stream Info
    struct parser_rec *rec;
    struct stream_info *info;
    if ((rec = parser_rec_get(parser, msg_ctx)) == NULL) {
        return IPX_ERR_NOMEM;
    }

    if ((info = stream_ctx_rec_get(&rec->ctx, msg_ctx->stream)) == NULL) {
        return IPX_ERR_NOMEM;
    }
    assert(rec->session == msg_ctx->session);
    assert(rec->odid == msg_ctx->odid);
    assert(info->id == msg_ctx->stream);

    // Check IPFIX Message header and its sequence number
    if (ntohs(msg_data->version) != FDS_IPFIX_VERSION) {
        PARSER_ERROR(plugin_ctx, msg_ctx, "IPFIX Message version doesn't match.");
        return IPX_ERR_FORMAT;
    }

    if (ntohs(msg_data->length) < FDS_IPFIX_MSG_HDR_LEN) {
        PARSER_ERROR(plugin_ctx, msg_ctx, "IPFIX Message Header size is invalid (total length of "
            "the message smaller than the IPFIX Message Header structure).");
        return IPX_ERR_FORMAT;
    }

    PARSER_DEBUG(plugin_ctx, msg_ctx, "Processing an IPFIX Message (Seq. number %" PRIu32 ")",
        ntohl(msg_data->seq_num));
    if ((info->flags & SIF_SEEN) != 0 && info->seq_num != ) {

    }

    // TODO: sequence number check

    // Configure a template manager
    fds_tmgr_t *tmgr = rec->ctx->mgr;
    rc = fds_tmgr_set_time(tmgr, ntohl(msg_data->export_time));
    switch (rc) {
    case FDS_OK:
        // Everything is OK
        break;
    case FDS_ERR_DENIED:
        // Failed to set Export Time
        PARSER_ERROR(plugin_ctx, msg_ctx, "Setting Export Time in history is not allowed for "
            "this type of Transport Session.");
        return IPX_ERR_FORMAT;
    case FDS_ERR_NOTFOUND:
        // Unable to find a corresponding snapshot
        PARSER_WARNING(plugin_ctx, msg_ctx, "Received IPFIX Message has too old Export Time. "
            "Template are no longer available. All its records are ignored.");
        return IPX_OK;
    case FDS_ERR_NOMEM:
        // Memory allocation failed
        PARSER_ERROR(plugin_ctx, msg_ctx, "Memory allocation failed in a template manager.");
        return IPX_ERR_NOMEM;
    default:
        // Unexpected error
        PARSER_ERROR(plugin_ctx, msg_ctx, "Template manager returned an unexpected error "
            "code (%d).", rc);
        return IPX_ERR_ARG;
    }

    // Parse IPFIX Sets
    struct parser_data parser_data;
    parser_data.ipfix_msg = ipfix_msg;
    parser_data.plugin_ctx = plugin_ctx;
    parser_data.tmgr = tmgr;

    process_msg(&parser_data);


    // TODO:
}






// TODO: kdo muze poslat pozadavek na odpojeni... jedine vlakno...

int
parser_init(ipx_ctx_t *ctx)
{
    ipx_parser_t *parser = ipx_parser_create(ctx);
    if (!parser) {
        IPX_ERROR(ctx, "Failed to create an IPFIX parser (%s:%d)", __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }

    ipx_ctx_set_private(ctx, parser);
}

int
parser_destroy(ipx_ctx_t *ctx)
{
    ipx_parser_t *parser = ipx_ctx_get_private(ctx);
    if (parser != NULL) {
        ipx_parser_destroy(parser);
    }

    return IPX_OK;
}

static int
parser_process_ipfix(ipx_ctx_t *ctx, ipx_parser_t *parser, ipx_msg_ipfix_t *msg)
{
    int rc = ipx_parser_process(parser, msg);
    switch (rc) {
    case IPX_OK;
        // Everything is ok...
        return IPX_OK;
    case IPX_ERR_FORMAT:
        if (ipx_ctx_feedback_enabled(ctx)) {
            const struct ipx_session *session = ipx_msg_ipfix_get_ctx(msg)->session;
            ipx_ctx_feedback_send(ctx, session);

        } else {

        }

        break;
    case IPX_ERR_NOMEM: {
        // Destroy all Template manager and close session
        const struct ipx_session *session = ipx_msg_ipfix_get_ctx(ipfix_msg)->session->ident);

        //
        ipx_parser_session_block(parser, session);

        IPX_ERROR(ctx, "Due to memory allocation failure all template managers of Transport "
            "Session has been removed.", ;
        ipx_ctx_feedback(ctx,);
    }
        break;
    default:

    }

    return rc;
}

static int
parser_process_session(ipx_ctx_t *ctx, ipx_parser_t *parser, ipx_msg_session_t *msg)
{
    int rc;

    // Process Session message
    if (ipx_msg_session_get_event(msg) != IPX_MSG_SESSION_CLOSE) {
        // Nothing to do...
        return IPX_OK;
    }

    const struct ipx_session *ts = ipx_msg_session_get_session(msg);
    rc = ipx_parser_session_remove(parser, ts);
    switch (rc) {
    case IPX_OK:
        IPX_INFO(ctx, "Template managers of Transport Session '%s' have been removed.", ts->ident);
        break;
    case IPX_ERR_NOTFOUND:
        IPX_INFO(ctx, "Template managers of Transport Session '%s' not found.", ts->ident);
        break;
    default:
        IPX_ERROR(ctx, "An unknown error (code %d) has occurred during Transport Session '%s' "
            "removal.", rc, ts->ident);
        break;
    }

    return IPX_OK;
}

void
parser_process(ipx_ctx_t *ctx, ipx_msg_t *msg)
{
    int rc;
    ipx_parser_t *parser = ipx_ctx_private_get(ctx);

    switch (ipx_msg_get_type(msg)) {
    case IPX_MSG_IPFIX:
        // Process an IPFIX message
        rc = parser_process_ipfix(ctx, parser, ipx_msg_base2ipfix(msg));
        break;
    case IPX_MSG_SESSION:
        // Process a Session message
        rc = parser_process_session(ctx, parser, ipx_msg_base2session(msg));
        break;
    default:
        // Pass unknown type of message
        rc = IPX_OK;
        break;
    }

    if (rc == IPX_OK) {
        ipx_ctx_msg_pass(ctx, msg);
    } else {
        ipx_msg_destroy(msg);
    }
}



