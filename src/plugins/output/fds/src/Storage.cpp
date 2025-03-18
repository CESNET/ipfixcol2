/**
 * \file src/plugins/output/fds/src/Storage.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \author Michal Sedlak <sedlakm@cesnet.cz>
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
#include <netinet/in.h>
#include <ipfixcol2.h>
#include <libgen.h>
#include "Storage.hpp"

const std::string TMP_SUFFIX = ".tmp";

static void
buf_write(std::vector<uint8_t> &buffer, const uint8_t *data, std::size_t data_len)
{
    if (data_len == 0) {
        return;
    }
    std::size_t idx = buffer.size();
    buffer.resize(idx + data_len);
    std::memcpy(&buffer[idx], data, data_len);
}

template <typename T>
static void
buf_write(std::vector<uint8_t> &buffer, T &&value)
{
    buf_write(buffer, reinterpret_cast<const uint8_t *>(&value), sizeof(value));
}

static bool
contains_element(const std::vector<Config::element> &elements, const fds_tfield &field)
{
    auto it = std::find_if(elements.begin(), elements.end(),
        [&](const Config::element &elem) { return elem.id == field.id && elem.pen == field.en; });
    return it != elements.end();
}

void
create_modified_template(const fds_template *tmplt,
                         const std::vector<Config::element> &selected_elements,
                         std::vector<uint8_t> &out_buffer)
{
    out_buffer.clear();

    // Collect the fields we want in the resulting template
    std::vector<const fds_tfield *> fields;
    for (uint16_t i = 0; i < tmplt->fields_cnt_total; i++) {
        const fds_tfield &field = tmplt->fields[i];
        if (contains_element(selected_elements, field)) {
            fields.push_back(&field);
        }
    }


    /**
    * Build the template definition...
    *
    * Template:
    *   [Template Record Header]
    *   [Field Specifier]
    *   [Field Specifier]
    *   ...
    *   [Field Specifier]
    *
    * Template Record Header:
    *   [Template ID : 16 bit] [Field Count : 16 bit]
    *
    * Example:
    *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    *   |      Template ID = 256        |         Field Count = N       |
    *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    *   |1| Information Element id. 1.1 |        Field Length 1.1       |
    *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    *   |                    Enterprise Number  1.1                     |
    *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    *   |0| Information Element id. 1.2 |        Field Length 1.2       |
    *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    *   |             ...               |              ...              |
    *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    *   |1| Information Element id. 1.N |        Field Length 1.N       |
    *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    *   |                    Enterprise Number  1.N                     |
    *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    *   |      Template ID = 257        |         Field Count = M       |
    *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    *   |0| Information Element id. 2.1 |        Field Length 2.1       |
    *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    *   |1| Information Element id. 2.2 |        Field Length 2.2       |
    *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    *   |                    Enterprise Number  2.2                     |
    *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    *   |             ...               |              ...              |
    *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    *   |1| Information Element id. 2.M |        Field Length 2.M       |
    *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    *   |                    Enterprise Number  2.M                     |
    *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    *   |                          Padding (opt)                        |
    *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
    */

    // Write the header
    buf_write(out_buffer, htons(tmplt->id));
    buf_write(out_buffer, htons(fields.size()));

    // Write the template fields
    for (const fds_tfield *field : fields) {
        if (field->en == 0) {
            buf_write(out_buffer, htons(field->id));
            buf_write(out_buffer, htons(field->length));
        } else {
            // If PEN is specified, MSB of element ID is set to 1
            buf_write(out_buffer, htons(field->id | 0x8000));
            buf_write(out_buffer, htons(field->length));
            buf_write(out_buffer, htonl(field->en));
        }
    }
}

static void
create_modified_data_record(fds_drec &drec,
                            const std::vector<Config::element> &selected_elements,
                            std::vector<uint8_t> &out_buffer)
{
    out_buffer.clear();

    /**
     * Data record consists of a sequence of field values.
     *
     * Data Record:
     *    +--------------------------------------------------+
     *    | Field Value                                      |
     *    +--------------------------------------------------+
     *    | Field Value                                      |
     *    +--------------------------------------------------+
     *     ...
     *    +--------------------------------------------------+
     *    | Field Value                                      |
     *    +--------------------------------------------------+
     */

    fds_drec_iter iter;
    fds_drec_iter_init(&iter, &drec, 0);
    while (fds_drec_iter_next(&iter) != FDS_EOC) {
        if (contains_element(selected_elements, *iter.field.info)) {
            if (iter.field.info->length == FDS_IPFIX_VAR_IE_LEN) {
              /**
               * Variable length fields are specified with 65535 in their
               * template field length. In this case, the first octet of the
               * field value in the data record specifies its length (of the
               * data only, the extra octet is not included in this length).
               *
               * If the data were to be longer than 254, the octet is 255 and the
               * next 2 octets are uint16 of the length.
               */
              if (iter.field.size < 255) {
                buf_write(out_buffer, uint8_t(iter.field.size));
                } else {
                    buf_write(out_buffer, uint8_t(255));
                    buf_write(out_buffer, htons(uint16_t(iter.field.size)));
                }
                buf_write(out_buffer, iter.field.data, iter.field.size);
            } else {
                buf_write(out_buffer, iter.field.data, iter.field.size);
            }
        }
    }
}

Storage::Storage(ipx_ctx_t *ctx, const Config &cfg) :
    m_ctx(ctx),
    m_path(cfg.m_path),
    m_selection_used(cfg.m_selection_used),
    m_selection(cfg.m_selection)
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

Storage::~Storage()
{
    window_close();
}

void
Storage::window_new(time_t ts)
{
    // Close the current window if exists
    window_close();

    // Open new file
    const std::string new_file = filename_gen(ts) + TMP_SUFFIX;
    m_file_name = new_file;
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
    bool file_opened = (m_file.get() != nullptr);
    m_file.reset();
    m_session2params.clear();
    if (file_opened) {
        std::string new_file_name(m_file_name.begin(), m_file_name.end() - TMP_SUFFIX.size());
        std::rename(m_file_name.c_str(), new_file_name.c_str());
        m_file_name.clear();
    }
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

        if (m_selection_used) {
            create_modified_data_record(rec_ptr->rec, m_selection, m_buffer);
            rec_data = m_buffer.data();
            rec_size = m_buffer.size();
            if (rec_size == 0) {
                // No record fields were selected -> the record is empty, skip
                continue;
            }
        }

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
    /// Whether selection is used
    bool selection_used;
    /// The selection
    std::vector<Config::element> *selection;
    /// Buffer for building modified templates
    std::vector<uint8_t> *buffer;
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
    auto info = reinterpret_cast<tmplt_update_data *>(data);

    // No exceptions can be thrown in the C callback!
    try {
        if (info->selection_used && tmplt->type != FDS_TYPE_TEMPLATE) {
            // Skip Option Templates in case field selection is used
            return info->is_ok;
        }

        // Get definition of the template we want to store
        fds_template_type new_t_type = tmplt->type;
        const uint8_t *new_t_data = tmplt->raw.data;
        uint16_t new_t_size = tmplt->raw.length;

        if (info->selection_used) {
            create_modified_template(tmplt, *info->selection, *info->buffer);
            new_t_data = info->buffer->data();
            new_t_size = info->buffer->size();

            if (new_t_size == 0) {
                // None of the template fields have been selected -> skip this template
                return info->is_ok;
            }
        }

        // Only now store the template ID as we're now sure that this is an active template
        info->ids.emplace(tmplt->id);

        // Get definition of the Template specified in the file
        fds_template_type old_t_type;
        const uint8_t *old_t_data;
        uint16_t old_t_size;

        int res = fds_file_write_tmplt_get(info->file, tmplt->id, &old_t_type, &old_t_data, &old_t_size);
        if (res != FDS_OK && res != FDS_ERR_NOTFOUND) {
            // Something bad happened
            const char *err_msg = fds_file_error(info->file);
            throw FDS_exception("fds_file_write_tmplt_get() failed: " + std::string(err_msg));
        }

        // Should we add/redefine the definition of the Template
        if (res == FDS_OK
                && old_t_type == new_t_type
                && old_t_size == new_t_size
                && memcmp(old_t_data, new_t_data, new_t_size) == 0) {
            // The same -> nothing to do
            return info->is_ok;
        }

        // Add the definition (i.e. templates are different or the template hasn't been defined)
        IPX_CTX_DEBUG(info->ctx, "Adding/updating definition of Template ID %" PRIu16, tmplt->id);

        if (fds_file_write_tmplt_add(info->file, new_t_type, new_t_data, new_t_size) != FDS_OK) {
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
    data.selection_used = m_selection_used;
    data.selection = &m_selection;
    data.buffer = &m_buffer;

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
