/**
 * \file src/plugins/output/fds/src/Storage.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief FDS file storage (source file)
 * \date June 2019
 *
 * Copyright(c) 2019 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <ipfixcol2.h>
#include <libgen.h>
#include "Storage.hpp"

Storage::Storage(ipx_ctx_t *ctx, const Config &cfg) : m_ctx(ctx), m_path(cfg.m_path)
{
    // Check if the directory exists
    struct stat file_info;
    memset(&file_info, 0, sizeof(file_info));
    if (stat(m_path.c_str(), &file_info) != 0 || !S_ISDIR(file_info.st_mode)) {
        throw FDS_exception("Directory '" + m_path + "' doesn't exist or search permission is denied");
    }

    // Prepare flags for FDS file
    m_flags = 0;
    switch (cfg.m_calg) {
    case Config::calg::LZ4:
        m_flags |= FDS_FILE_LZ4;
        break;
    case Config::calg::ZSTD:
        m_flags |= FDS_FILE_ZSTD;
        break;
    default:
        break;
    }

    if (!cfg.m_async) {
        m_flags |= FDS_FILE_NOASYNC;
    }

    m_flags |= FDS_FILE_APPEND;
}

void
Storage::window_new(time_t ts)
{
    // Close the current window if exists
    window_close();

    // Open new file
    const std::string new_file = filename_gen(ts);
    std::unique_ptr<char, decltype(&free)> new_file_cpy(strdup(new_file.c_str()), &free);

    char *dir2create;
    if (!new_file_cpy || (dir2create = dirname(new_file_cpy.get())) == nullptr) {
        throw FDS_exception("Failed to generate name of an output directory!");
    }

    if (ipx_utils_mkdir(dir2create, IPX_UTILS_MKDIR_DEF) != FDS_OK) {
        throw FDS_exception("Failed to create directory '" + std::string(dir2create) + "'");
    }

    m_file.reset(fds_file_init());
    if (!m_file) {
        throw FDS_exception("Failed to create FDS file handler!");
    }

    if (fds_file_open(m_file.get(), new_file.c_str(), m_flags) != FDS_OK) {
        std::string err_msg = fds_file_error(m_file.get());
        m_file.reset();
        throw FDS_exception("Failed to create/append file '" + new_file + "': " + err_msg);
    }
}

void
Storage::window_close()
{
    m_file.reset();
    m_session2params.clear();
}

void
Storage::process_msg(ipx_msg_ipfix_t *msg)
{
    if (!m_file) {
        IPX_CTX_DEBUG(m_ctx, "Ignoring IPFIX Message due to undefined output file!", '\0');
        return;
    }

    // Specify a Transport Session context
    struct ipx_msg_ctx *msg_ctx = ipx_msg_ipfix_get_ctx(msg);
    session_ctx &file_ctx = session_get(msg_ctx->session);

    auto hdr_ptr = reinterpret_cast<fds_ipfix_msg_hdr *>(ipx_msg_ipfix_get_packet(msg));
    assert(ntohs(hdr_ptr->version) == FDS_IPFIX_VERSION && "Unexpected packet version");
    const uint32_t exp_time = ntohl(hdr_ptr->export_time);

    if (fds_file_write_ctx(m_file.get(), file_ctx.id, msg_ctx->odid, exp_time) != FDS_OK) {
        const char *err_msg = fds_file_error(m_file.get());
        throw FDS_exception("Failed to configure the writer: " + std::string(err_msg));
    }

    // Get info about the last seen Template snapshot
    struct snap_info &snap_last = file_ctx.odid2snap[msg_ctx->odid];

    // For each Data Record in the file
    const uint32_t rec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);
    for (uint32_t i = 0; i < rec_cnt; ++i) {
        ipx_ipfix_record *rec_ptr = ipx_msg_ipfix_get_drec(msg, i);

        // Check if the templates has been changed (detected by change of template snapshots)
        if (rec_ptr->rec.snap != snap_last.ptr) {
            const char *session_name = msg_ctx->session->ident;
            uint32_t session_odid = msg_ctx->odid;
            IPX_CTX_DEBUG(m_ctx, "Template snapshot of '%s' [ODID %" PRIu32 "] has been changed. "
                "Updating template definitions...", session_name, session_odid);

            tmplts_update(snap_last, rec_ptr->rec.snap);
        }

        // Write the Data Record
        const uint8_t *rec_data = rec_ptr->rec.data;
        uint16_t rec_size = rec_ptr->rec.size;
        uint16_t tmplt_id = rec_ptr->rec.tmplt->id;

        if (fds_file_write_rec(m_file.get(), tmplt_id, rec_data, rec_size) != FDS_OK) {
            const char *err_msg = fds_file_error(m_file.get());
            throw FDS_exception("Failed to add a Data Record: " + std::string(err_msg));
        }
    }
}

/// Auxiliary data structure used in the snapshot iterator
struct tmplt_update_data {
    /// Status of template processing
    bool is_ok;
    /// Plugin context (only for log!)
    ipx_ctx_t *ctx;

    /// FDS file with specified context
    fds_file_t *file;
    /// Set of processed Templates in the snapshot
    std::set<uint16_t> ids;
};

/**
 * @brief Callback function for updating definition of an IPFIX (Options) Template
 *
 * The function checks if the same Template is already defined in the current context of the file.
 * If the Template is not present or it's different, the new Template definition is added to the
 * file.
 * @param[in] tmplt Template to process
 * @param[in] data  Auxiliary data structure \ref tmplt_update_data
 * @return On success returns true. Otherwise returns false.
 */
static bool
tmplt_update_cb(const struct fds_template *tmplt, void *data)
{
    // Template type, raw data and size
    enum fds_template_type t_type;
    const uint8_t *t_data;
    uint16_t t_size;

    auto info = reinterpret_cast<tmplt_update_data *>(data);

    // No exceptions can be thrown in the C callback!
    try {
        uint16_t t_id = tmplt->id;
        info->ids.emplace(t_id);

        // Get definition of the Template specified in the file
        int res = fds_file_write_tmplt_get(info->file, t_id, &t_type, &t_data, &t_size);

        if (res != FDS_OK && res != FDS_ERR_NOTFOUND) {
            // Something bad happened
            const char *err_msg = fds_file_error(info->file);
            throw FDS_exception("fds_file_write_tmplt_get() failed: " + std::string(err_msg));
        }

        // Should we add/redefine the definition of the Template
        if (res == FDS_OK
                && tmplt->type == t_type
                && tmplt->raw.length == t_size
                && memcmp(tmplt->raw.data, t_data, t_size) == 0) {
            // The same -> nothing to do
            return info->is_ok;
        }

        // Add the definition (i.e. templates are different or the template hasn't been defined)
        IPX_CTX_DEBUG(info->ctx, "Adding/updating definition of Template ID %" PRIu16, t_id);

        t_type = tmplt->type;
        t_data = tmplt->raw.data;
        t_size = tmplt->raw.length;

        if (fds_file_write_tmplt_add(info->file, t_type, t_data, t_size) != FDS_OK) {
            const char *err_msg = fds_file_error(info->file);
            throw FDS_exception("fds_file_write_tmplt_add() failed: " + std::string(err_msg));
        }

    } catch (std::exception &ex) {
        // Exceptions
        IPX_CTX_ERROR(info->ctx, "Failure during update of Template ID %" PRIu16 ": %s", tmplt->id,
            ex.what());
        info->is_ok = false;
    } catch (...) {
        // Other exceptions
        IPX_CTX_ERROR(info->ctx, "Unknown exception thrown during template definition update", '\0');
        info->is_ok = false;
    }

    return info->is_ok;
}

/**
 * @brief Update Template definitions for the current Transport Session and ODID
 *
 * The function compares Templates in the \p snap with Template definitions previously defined
 * for the currently selected combination of Transport Session and ODID. For each different or
 * previously undefined Template, its definition is added or updated. Definitions of Templates
 * that were available in the previous snapshot but not available in the new one are removed.
 *
 * Finally, information (pointer, IDs) in \p info are updated to reflect the performed update.
 * @warning
 *   Template definitions are always unique for a combination of Transport Session and ODID,
 *   therefore, appropriate file writer context MUST already set using fds_file_writer_ctx().
 *   Parameters \p info and \p snap MUST also belong the same unique combination.
 * @param[in] info Information about the last update of Templates (old snapshot ref. + list of IDs)
 * @param[in] snap New Template snapshot with all valid Template definitions
 */
void
Storage::tmplts_update(struct snap_info &info, const fds_tsnapshot_t *snap)
{
    assert(info.ptr != snap && "Snapshots should be different");

    // Prepare data for the callback function
    struct tmplt_update_data data;
    data.is_ok = true;
    data.ctx = m_ctx;
    data.file = m_file.get();
    data.ids.clear();

    // Update templates
    fds_tsnapshot_for(snap, &tmplt_update_cb, &data);

    // Check if the update failed
    if (!data.is_ok) {
        throw FDS_exception("Failed to update Template definitions");
    }

    // Check if there are any Template IDs that have been removed
    std::set<uint16_t> &ids_old = info.tmplt_ids;
    std::set<uint16_t> &ids_new = data.ids;
    std::set<uint16_t> ids2remove;
    // Old Template IDs - New Templates IDs = Template IDs to remove
    std::set_difference(ids_old.begin(), ids_old.end(), ids_new.begin(), ids_new.end(),
        std::inserter(ids2remove, ids2remove.begin()));

    // Remove old templates that are not available in the new snapshot
    for (uint16_t tid : ids2remove) {
        IPX_CTX_DEBUG(m_ctx, "Removing definition of Template ID %" PRIu16, tid);

        int rc = fds_file_write_tmplt_remove(m_file.get(), tid);
        if (rc == FDS_OK) {
            continue;
        }

        // Something bad happened
        if (rc != FDS_ERR_NOTFOUND) {
            std::string err_msg = fds_file_error(m_file.get());
            throw FDS_exception("fds_file_write_tmplt_remove() failed: " + err_msg);
        }

        // Weird, but not critical
        IPX_CTX_WARNING(m_ctx, "Failed to remove undefined Template ID %" PRIu16 ". "
            "Weird, this should not happen.", tid);
    }

    // Update information about the last update of Templates
    info.ptr = snap;
    std::swap(info.tmplt_ids, data.ids);
}

/**
 * @brief Create a filename based for a user defined timestamp
 * @note The timestamp will be expressed in Coordinated Universal Time (UTC)
 *
 * @param[in] ts Timestamp of the file
 * @return New filename
 * @throw FDS_exception if formatting functions fail.
 */
std::string
Storage::filename_gen(const time_t &ts)
{
    const char pattern[] = "%Y/%m/%d/flows.%Y%m%d%H%M%S.fds";
    constexpr size_t buffer_size = 64;
    char buffer_data[buffer_size];

    struct tm utc_time;
    if (!gmtime_r(&ts, &utc_time)) {
        throw FDS_exception("gmtime_r() failed");
    }

    if (strftime(buffer_data, buffer_size, pattern, &utc_time) == 0) {
        throw FDS_exception("strftime() failed");
    }

    std::string new_path = m_path;
    if (new_path.back() != '/') {
        new_path += '/';
    }

    return new_path + buffer_data;
}

/**
 * @brief Convert IPv4 address to IPv4-mapped IPv6 address
 *
 * New IPv6 address has value corresponding to "::FFFF:\<IPv4\>"
 * @warning Size of @p in must be at least 4 bytes and @p out at least 16 bytes!
 * @param[in]  in  IPv4 address
 * @param[out] out IPv4-mapped IPv6 address
 */
void
Storage::ipv4toipv6(const uint8_t *in, uint8_t *out)
{
    memset(out, 0, 16U);
    *(uint16_t *) &out[10] = UINT16_MAX;  // 0xFFFF
    memcpy(&out[12], in, 4U);             // Copy IPv4 address
}

/**
 * @brief Get file identification of a Transport Session
 *
 * If the identification doesn't exist, the function will add it to the file and create
 * a new internal record for it.
 *
 * @param[in] sptr Transport Session to find
 * @return Internal description
 * @throw FDS_exception if the function failed to add a new Transport Session
 */
struct Storage::session_ctx &
Storage::session_get(const struct ipx_session *sptr)
{
    auto res_it = m_session2params.find(sptr);
    if (res_it != m_session2params.end()) {
        // Found
        return res_it->second;
    }

    // Not found -> register a new session
    assert(m_file != nullptr && "File must be opened!");

    struct fds_file_session new_session;
    fds_file_sid_t new_sid;

    session_ipx2fds(sptr, &new_session);
    if (fds_file_session_add(m_file.get(), &new_session, &new_sid) != FDS_OK) {
        const char *err_msg = fds_file_error(m_file.get());
        throw FDS_exception("Failed to register Transport Session '" + std::string(sptr->ident)
            + "': " + err_msg);
    }

    // Create a new session
    struct session_ctx &ctx = m_session2params[sptr];
    ctx.id = new_sid;
    return ctx;
}

/**
 * @brief Convert IPFIXcol representation of a Transport Session to FDS representation
 * @param[in]  ipx_desc IPFIXcol specific representation
 * @param[out] fds_desc FDS specific representation
 * @throw FDS_exception if conversion fails due to unsupported Transport Session type
 */
void
Storage::session_ipx2fds(const struct ipx_session *ipx_desc, struct fds_file_session *fds_desc)
{
    // Initialize Transport Session structure
    memset(fds_desc, 0, sizeof *fds_desc);

    // Extract protocol type and description
    const struct ipx_session_net *net_desc = nullptr;
    switch (ipx_desc->type) {
    case FDS_SESSION_UDP:
        net_desc = &ipx_desc->udp.net;
        fds_desc->proto = FDS_FILE_SESSION_UDP;
        break;
    case FDS_SESSION_TCP:
        net_desc = &ipx_desc->tcp.net;
        fds_desc->proto = FDS_FILE_SESSION_TCP;
        break;
    case FDS_SESSION_SCTP:
        net_desc = &ipx_desc->sctp.net;
        fds_desc->proto = FDS_FILE_SESSION_SCTP;
        break;
    default:
        throw FDS_exception("Not implemented Transport Session type!");
    }

    // Convert ports
    fds_desc->port_src = net_desc->port_src;
    fds_desc->port_dst = net_desc->port_dst;

    // Convert IP addresses
    if (net_desc->l3_proto == AF_INET) {
        // IPv4 address
        static_assert(sizeof(net_desc->addr_src.ipv4) >= 4U, "Invalid size");
        static_assert(sizeof(net_desc->addr_dst.ipv4) >= 4U, "Invalid size");
        ipv4toipv6(reinterpret_cast<const uint8_t *>(&net_desc->addr_src.ipv4), fds_desc->ip_src);
        ipv4toipv6(reinterpret_cast<const uint8_t *>(&net_desc->addr_dst.ipv4), fds_desc->ip_dst);
    } else {
        // IPv6 address
        static_assert(sizeof(fds_desc->ip_src) <= sizeof(net_desc->addr_src.ipv6), "Invalid size");
        static_assert(sizeof(fds_desc->ip_dst) <= sizeof(net_desc->addr_dst.ipv6), "Invalid size");
        memcpy(&fds_desc->ip_src, &net_desc->addr_src.ipv6, sizeof fds_desc->ip_src);
        memcpy(&fds_desc->ip_dst, &net_desc->addr_dst.ipv6, sizeof fds_desc->ip_dst);
    }
}

