/**
 * \file src/plugins/output/fds/src/Storage.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief FDS file storage (header file)
 * \date June 2019
 *
 * Copyright(c) 2019 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IPFIXCOL2_FDS_STORAGE_HPP
#define IPFIXCOL2_FDS_STORAGE_HPP

#include <ipfixcol2.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <libfds.h>

#include "Exception.hpp"
#include "Config.hpp"

/// Flow storage file
class Storage {
public:
    /**
     * @brief Create a flow storage file
     *
     * @note
     *   Output file for the current window MUST be specified using new_window() function.
     *   Otherwise, no flow records are stored.
     *
     * @param[in] ctx  Plugin context (only for log)
     * @param[in] cfg  Configuration
     * @throw FDS_exception if @p path directory doesn't exist in the system
     */
    Storage(ipx_ctx_t *ctx, const Config &cfg);
    virtual ~Storage() = default;

    // Disable copy constructors
    Storage(const Storage &other) = delete;
    Storage &operator=(const Storage &other) = delete;

    /**
     * @brief Create a new time window
     *
     * @note Previous window is automatically closed, if exists.
     * @param[in] ts Timestamp of the window
     * @throw FDS_exception if the new window cannot be created
     */
    void
    window_new(time_t ts);

    /**
     * @brief Close the current time window
     * @note
     *   This can be also useful if a fatal error has occurred and we should not add more flow
     *   records to the file.
     * @note
     *   No more Data Records will be added until a new window is created!
     */
    void
    window_close();

    /**
     * @brief Process IPFIX message
     *
     * Process all IPFIX Data Records in the message and store them to the file.
     * @note If a time window is not opened, no Data Records are stored and no exception is thrown.
     * @param[in] msg Message to process
     * @throw FDS_exception if processing fails
     */
    void
    process_msg(ipx_msg_ipfix_t *msg);

private:
    /// Information about Templates in a snapshot
    struct snap_info {
        /// Last seen snapshot (might be already freed, do NOT dereference!)
        const fds_tsnapshot_t *ptr;
        /// Set of Template IDs in the snapshot
        std::set<uint16_t> tmplt_ids;

        snap_info() {
            ptr = nullptr;
            tmplt_ids.clear();
        }
    };

    /// Description parameters of a Transport Session
    struct session_ctx {
        /// Session ID used in the FDS file
        fds_file_sid_t id;
        /// Last seen snapshot for a specific ODID of the Transport Session
        std::map<uint32_t, struct snap_info> odid2snap;
    };

    /// Plugin context only for logging!
    ipx_ctx_t *m_ctx;
    /// Storage path
    std::string m_path;
    /// Flags for opening file
    uint32_t m_flags;

    /// Output FDS file
    std::unique_ptr<fds_file_t, decltype(&fds_file_close)> m_file = {nullptr, &fds_file_close};
    /// Mapping of Transport Sessions to FDS specific parameters
    std::map<const struct ipx_session *, struct session_ctx> m_session2params;

    std::string
    filename_gen(const time_t &ts);
    static void
    ipv4toipv6(const uint8_t *in, uint8_t *out);
    struct session_ctx &
    session_get(const struct ipx_session *sptr);
    void
    session_ipx2fds(const struct ipx_session *ipx_desc, struct fds_file_session *fds_desc);
    void
    tmplts_update(struct snap_info &info, const fds_tsnapshot_t *snap);
};


#endif // IPFIXCOL2_FDS_STORAGE_HPP
