/**
 * \file src/plugins/output/fds/src/Storage.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief FDS file storage (source file)
 * \date June 2019
 *
 * Copyright(c) 2019 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

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
        m_file.reset();
        const char *err_msg = "something"; //fds_file_error(m_file.get());
        throw FDS_exception("Failed to create file '" + new_file + "': " + err_msg);
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
        throw FDS_exception("Failed to configure file writer: " + std::string(err_msg));
    }

    // For each Data Record in the file
    const uint32_t rec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);
    for (uint32_t i = 0; i < rec_cnt; ++i) {
        ipx_ipfix_record *rec_ptr = ipx_msg_ipfix_get_drec(msg, i);

        // Insert the record // TODO: improve me!
        const struct fds_template *rec_tmplt = rec_ptr->rec.tmplt;
        uint16_t tmplt_id = rec_tmplt->id;
        enum fds_template_type t_type;
        const uint8_t *t_data;
        uint16_t t_size;

        int rc = fds_file_write_tmplt_get(m_file.get(), tmplt_id, &t_type, &t_data, &t_size);
        if (rc != FDS_OK && rc != FDS_ERR_NOTFOUND) {
            // Something bad happened
            const char *err_msg = fds_file_error(m_file.get());
            throw FDS_exception("fds_file_write_tmplt_get() failed: " + std::string(err_msg));
        }

        if (rc == FDS_ERR_NOTFOUND || t_type != rec_tmplt->type || t_size != rec_tmplt->raw.length
                || memcmp(t_data, rec_tmplt->raw.data, rec_tmplt->raw.length) != 0) {
            // Template not defined or the template are different
            t_type = rec_tmplt->type;
            t_data = rec_tmplt->raw.data;
            t_size = rec_tmplt->raw.length;

            if (fds_file_write_tmplt_add(m_file.get(), t_type, t_data, t_size) != FDS_OK) {
                const char *err_msg = fds_file_error(m_file.get());
                throw FDS_exception("Failed to add a template: " + std::string(err_msg));
            }
        }

        // FIXME: check subTemplateList & subTemplateMultiList templates

        // Write the Data record
        const uint8_t *rec_data = rec_ptr->rec.data;
        const uint16_t rec_size = rec_ptr->rec.size;
        if (fds_file_write_rec(m_file.get(), tmplt_id, rec_data, rec_size) != FDS_OK) {
            const char *err_msg = fds_file_error(m_file.get());
            throw FDS_exception("Failed to add a Data Record: " + std::string(err_msg));
        }
    }
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

