/**
 * \file src/plugins/input/fds/Reader.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief FDS file reader (implementation)
 * \date May 2020
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

#include <cstring>
#include <cinttypes>
#include <vector>

#include "Builder.hpp"
#include "Exception.hpp"
#include "Reader.hpp"


Reader::Reader(ipx_ctx_t *ctx, const fds_config *cfg, const char *path)
    : m_ctx(ctx), m_cfg(cfg)
{
    uint32_t flags = FDS_FILE_READ;
    flags |= (m_cfg->async) ? 0 : FDS_FILE_NOASYNC;

    m_file.reset(fds_file_init());
    if (!m_file) {
        throw FDS_exception("fds_file_init() failed!");
    }

    if (fds_file_open(m_file.get(), path, flags) != FDS_OK) {
        throw FDS_exception("Unable to open file '" + std::string(path));
    }
}

Reader::~Reader()
{
    // Send notification about closing of all Transport Sessions
    for (auto &it : m_sessions) {
        session_close(it.second.info);
        it.second.info = nullptr;
    }
}

/**
 * @brief Get a Transport Session description given by FDS (Transport) Session ID
 *
 * The Session description is extracted from the FDS File and converted to particular
 * IPFIXcol structure and returned.
 * @param[in] sid Session ID
 * @return Transport Session
 * @throw FDS_exception in case of a failure (e.g. invalid ID, memory allocation error)
 */
struct ipx_session *
Reader::session_from_sid(fds_file_sid_t sid)
{
    const struct fds_file_session *desc;
    struct ipx_session *session;
    struct ipx_session_net session_net;

    if (fds_file_session_get(m_file.get(), sid, &desc) != FDS_OK) {
        throw FDS_exception("Unable to get Transport Session with ID " + std::to_string(sid));
    }

    // Convert FDS structure to IPFIXcol structure
    memset(&session_net, 0, sizeof(session_net));
    session_net.port_src = desc->port_src;
    session_net.port_dst = desc->port_dst;
    if (IN6_IS_ADDR_V4MAPPED(desc->ip_src) && IN6_IS_ADDR_V4MAPPED(desc->ip_dst)) {
        session_net.l3_proto = AF_INET;
        session_net.addr_src.ipv4 = *reinterpret_cast<const struct in_addr *>(&desc->ip_src[12]);
        session_net.addr_dst.ipv4 = *reinterpret_cast<const struct in_addr *>(&desc->ip_dst[12]);
    } else {
        session_net.l3_proto = AF_INET6;
        session_net.addr_src.ipv6 = *reinterpret_cast<const struct in6_addr *>(&desc->ip_src[0]);
        session_net.addr_dst.ipv6 = *reinterpret_cast<const struct in6_addr *>(&desc->ip_dst[0]);
    }

    switch (desc->proto) {
    case FDS_FILE_SESSION_UDP:
        session = ipx_session_new_udp(&session_net, UINT16_MAX, UINT16_MAX);
        break;
    case FDS_FILE_SESSION_TCP:
        session = ipx_session_new_tcp(&session_net);
        break;
    case FDS_FILE_SESSION_SCTP:
        session = ipx_session_new_sctp(&session_net);
        break;
    case FDS_FILE_SESSION_UNKNOWN:
        session = ipx_session_new_file(("UnknownSID<" + std::to_string(sid) + ">").c_str());
        break;
    default:
        throw FDS_exception("Unknown FDS session type: " + std::to_string(desc->proto));
    }

    if (!session) {
        throw  FDS_exception("Failed to create a Transport Session "
            "(probably a memory allocation error)");
    }

    return session;
}

/**
 * @brief Notify other plugins about a new Transport Session
 *
 * A new Session Message is generated and send to other plugins in the pipeline.
 * @param[in] ts Transport Session
 * @throw FDS_exception in case of failure
 */
void
Reader::session_open(struct ipx_session *ts)
{
    struct ipx_msg_session *msg;

    // Notify plugins further in the pipeline about the new session
    msg = ipx_msg_session_create(ts, IPX_MSG_SESSION_OPEN);
    if (!msg) {
        throw FDS_exception("Failed to create a Transport Session notification");
    }

    if (ipx_ctx_msg_pass(m_ctx, ipx_msg_session2base(msg)) != IPX_OK) {
        ipx_msg_session_destroy(msg);
        throw  FDS_exception("Failed to pass a Transport Session notification");
    }
}

/**
 * @brief Notify other plugins about a close of a Transport Session
 *
 * @warning
 *   User MUST stop using the Session as it is send in a garbage message to the
 *   pipeline and it will be automatically freed later.
 * @param[in] ts Transport Session
 * @throw FDS_exception in case of failure
 */
void
Reader::session_close(struct ipx_session *ts)
{
    ipx_msg_session_t *msg_session;
    ipx_msg_garbage_t *msg_garbage;
    ipx_msg_garbage_cb garbage_cb = (ipx_msg_garbage_cb) &ipx_session_destroy;

    msg_session = ipx_msg_session_create(ts, IPX_MSG_SESSION_CLOSE);
    if (!msg_session) {
        throw FDS_exception("Failed to create a Transport Session notification");
    }

    if (ipx_ctx_msg_pass(m_ctx, ipx_msg_session2base(msg_session)) != IPX_OK) {
        ipx_msg_session_destroy(msg_session);
        throw FDS_exception("Failed to pass a Transport Session notification");
    }

    msg_garbage = ipx_msg_garbage_create(ts, garbage_cb);
    if (!msg_garbage) {
        /* Memory leak... We cannot destroy the session as it can be used
         * by other plugins further in the pipeline. */
        throw FDS_exception("Failed to create a garbage message with a Transport Session");
    }

    if (ipx_ctx_msg_pass(m_ctx, ipx_msg_garbage2base(msg_garbage)) != IPX_OK) {
        /* Memory leak... We cannot destroy the message as it also destroys
         * the session structure. */
        throw FDS_exception("Failed to pass a garbage message with a Transport Session");
    }
}

/// Auxiliary data for snapshot interator callback
struct tmplt_cb_data {
    std::vector<Builder> msg_vec;    ///< Vector of generated IPFIX Messages
    uint16_t msg_size;               ///< Allocation size of IPFIX Messages
    uint32_t odid;                   ///< Observation Domain ID
    uint32_t exp_time;               ///< Export Time
    uint32_t seq_num;                ///< Sequence number

    bool is_ok;                      ///< Failure indicator
};

/**
 * Snapshot iterator callback
 *
 * The purpose of this function is to add an (Options) Template to the current
 * IPFIX Message or create a new one, if doesn't exist or it is full.
 *
 * @note
 *   The callback assumes that there is already at least one IPFIX builder prepared!
 * @param[in] tmplt Template to add
 * @param[in] data  Callback data
 * @return True on success
 * @return False in case of a fatal failure
 */
static bool
tmplt_cb_func(const struct fds_template *tmplt, void *data) noexcept
{
    auto *cb_data = reinterpret_cast<struct tmplt_cb_data *>(data);

    // Callback MUST NOT throw an exception!
    try {
        // Try to insert (Options) Template to the current IPFIX Message
        Builder *msg_ptr = &cb_data->msg_vec.back();
        if (msg_ptr->add_template(tmplt)) {
            return true; // Success
        }

        // Create a new IPFIX Message (the previous one is full)
        cb_data->msg_vec.emplace_back(cb_data->msg_size);
        msg_ptr = &cb_data->msg_vec.back();
        if (msg_ptr->add_template(tmplt)) {
            return true; // Success
        }

        // The (Options) Template doesn't fit into an empty IPFIX Message
        msg_ptr->resize(UINT16_MAX);
        if (msg_ptr->add_template(tmplt)) {
            return true; // Success
        }

        // This is really bad
        cb_data->is_ok = false;
        return false;
    } catch (...) {
        cb_data->is_ok = false;
        return false;
    }
}

/**
 * Generate and send one or more IPFIX Messages with all (Options) Templates
 *
 * Extract all (Options) Templates from a Template Snapshot, generate IPFIX Messages
 * and sent the to the pipeline.
 * @param[in] ts       Transport Session
 * @param[in] tsnap    Template snapshot
 * @param[in] odid     Observation Domain ID (of the IPFIX Message(s))
 * @param[in] exp_time Export Time (of the IPFIX Message(s))
 * @param[in] seq_num  Sequence Number (of the IPFIX Message(s))
 */
void
Reader::send_templates(const struct ipx_session *ts, const fds_tsnapshot_t *tsnap,
    uint32_t odid, uint32_t exp_time, uint32_t seq_num)
{
    // Prepare data for an iterator callback
    struct tmplt_cb_data cb_data;
    cb_data.msg_size = m_cfg->msize;
    cb_data.odid = odid;
    cb_data.exp_time = exp_time;
    cb_data.seq_num = seq_num;
    cb_data.is_ok = true;

    // Create an emptry IPFIX Message builder
    cb_data.msg_vec.emplace_back(cb_data.msg_size);

    if (ts->type != FDS_SESSION_UDP) {
        // Withdraw all (Options) Templates first
        cb_data.msg_vec.back().add_withdrawals();
    }

    // Generate one or more IPFIX Messages with (Options) Templates
    fds_tsnapshot_for(tsnap, &tmplt_cb_func, &cb_data);
    if (!cb_data.is_ok) {
        throw FDS_exception("Failed to generate IPFIX Message(s) with Templates!");
    }

    for (auto &msg : cb_data.msg_vec) {
        // Update IPFIX Message header
        msg.set_etime(exp_time);
        msg.set_odid(odid);
        msg.set_seqnum(seq_num);

        // Send it
        send_ipfix(msg.release(), ts, odid);
    }
}

/**
 * Send an IPFIX Message to the pipeline
 *
 * @note
 *   The function takes responsibility for the Message. Therefore, in case of
 *   failure, the Message will be freed.
 * @param[in] msg  Raw IPFIX Message to send
 * @param[in] ts   Transport Session
 * @param[in] odid Observation Domain ID (of the message)
 * @throw FDS_exception in case of failure
 */
void
Reader::send_ipfix(uint8_t *msg, const struct ipx_session *ts, uint32_t odid)
{
    uint16_t msg_size = ntohs(reinterpret_cast<fds_ipfix_msg_hdr *>(msg)->length);
    ipx_msg_ipfix_t *msg_ptr;
    struct ipx_msg_ctx msg_ctx;

    msg_ctx.session = ts;
    msg_ctx.odid = odid;
    msg_ctx.stream = 0; // stream is not stored in the file

    msg_ptr = ipx_msg_ipfix_create(m_ctx, &msg_ctx, msg, msg_size);
    if (!msg_ptr) {
        free(msg);
        throw FDS_exception("Failed to allocate an IPFIX Message!");
    }

    // Send it to the pipeline
    if (ipx_ctx_msg_pass(m_ctx, ipx_msg_ipfix2base(msg_ptr)) != IPX_OK) {
        ipx_msg_ipfix_destroy(msg_ptr);
        throw FDS_exception("Failed to pass an IPFIX Message!");
    }
}

/**
 * @brief Get the next Data Record to process
 *
 * @warning
 *   The function will return still the same record until @p m_unproc is set to false!
 * @param[out] rec Pointer to the Data Record
 * @param[out] ctx Pointer to the context of the Data Record (ODID, Export Time, etc.)
 * @return #IPX_OK on success
 * @return #IPX_ERR_EOF if there are no more records in the file
 * @throw FDS_exception in case of failure
 */
int
Reader::record_get(const struct fds_drec **rec, const struct fds_file_read_ctx **ctx)
{
    int ret;

    if (m_unproc) {
        // Return previously unprocessed Data Record
        *rec = &m_unproc_data;
        *ctx = &m_unproc_ctx;
        return IPX_OK;
    }

    ret = fds_file_read_rec(m_file.get(), &m_unproc_data, &m_unproc_ctx);
    switch (ret) {
    case FDS_OK:  // Success
        break;
    case FDS_EOC: // End of file
        return IPX_ERR_EOF;
    default:
        throw FDS_exception("fds_file_read_rec() failed: "
            + std::string(fds_file_error(m_file.get())));
    }

    *rec = &m_unproc_data;
    *ctx = &m_unproc_ctx;
    m_unproc = true;
    return IPX_OK;
}

int
Reader::send_batch()
{
    Builder new_msg(m_cfg->msize);
    fds_file_sid_t msg_sid;
    uint32_t msg_odid;
    uint32_t msg_etime;
    uint32_t msg_seqnum;
    uint16_t rec_cnt = 0;

    struct Session *ptr_session = nullptr;
    struct ODID *ptr_odid = nullptr;

    const struct fds_drec *drec;
    const struct fds_file_read_ctx *dctx;

    // Get the first Data Record
    switch (record_get(&drec, &dctx)) {
    case IPX_OK:
        break;
    case IPX_ERR_EOF:
        return IPX_ERR_EOF;
    default:
        throw FDS_exception("[internal] record_get() returned unexpected value!");
    }

    // Prepare contextual information for the IPFIX Message to generate
    msg_sid = dctx->sid;
    msg_odid = dctx->odid;
    msg_etime = dctx->exp_time;

    ptr_session = &m_sessions[msg_sid];
    ptr_odid = &ptr_session->odids[msg_odid];
    msg_seqnum = ptr_odid->seq_num;

    // Make sure that Session is already defined and Templates are ok...
    if (!ptr_session->info) {
        ptr_session->info = session_from_sid(msg_sid);
        IPX_CTX_DEBUG(m_ctx, "New TS '%s' detected!", ptr_session->info->ident);
        session_open(ptr_session->info);
    }

    if (ptr_odid->tsnap != drec->snap) {
        IPX_CTX_DEBUG(m_ctx, "Sending all (Options) Templates of '%s:%" PRIu32 "'",
            ptr_session->info->ident, msg_odid);
        send_templates(ptr_session->info, drec->snap, msg_odid, msg_etime, msg_seqnum);
        ptr_odid->tsnap = drec->snap;
    }

    // Try to insert the first Data Record to the IPFIX Message
    if (!new_msg.add_record(drec)) {
        // The Data Record doesn't fit into an empty IPFIX Message!
        new_msg.resize(UINT16_MAX);

        // Try again
        if (!new_msg.add_record(drec)) {
            throw FDS_exception("[internal] Failed to insert a Data Record into an IPFIX Message!");
        }
    }

    // Consider the Data Record as successfully processed!
    m_unproc = false;
    rec_cnt += 1;

    /* Since we know that FDS File stores all Data Records in blocks, where each block
     * contains records from the same context (i.e. Transport Session and ODID) and
     * the records are described by the same templates, we can assume that consecutive
     * read operations will be most likely return Data Records which share the same features
     * and, therefore, they can be added to the same IPFIX Message. */
    while (true) {
        int ret = record_get(&drec, &dctx);
        if (ret != IPX_OK) {
            // Probably end of file...
            break;
        }

        if (msg_sid != dctx->sid
                || msg_odid != dctx->odid
                || msg_etime != dctx->exp_time // due to relative timestamps in Data Record
                || drec->snap != ptr_odid->tsnap) {
            // The Data Record doesn't belong to this IPFIX Message... flush it!
            break;
        }

        if (!new_msg.add_record(drec)) {
            // The IPFIX Message is full
            break;
        }

        m_unproc = false;
        rec_cnt++;
    }

    // Update IPFIX Message header and send it!
    new_msg.set_etime(msg_etime);
    new_msg.set_odid(msg_odid);
    new_msg.set_seqnum(msg_seqnum);
    ptr_odid->seq_num += rec_cnt;

    send_ipfix(new_msg.release(), ptr_session->info, msg_odid);
    IPX_CTX_DEBUG(m_ctx, "New IPFIX Message with %" PRIu16 " records from '%s:%" PRIu32 "' sent!",
        rec_cnt, ptr_session->info->ident, msg_odid);
    return IPX_OK;
}
