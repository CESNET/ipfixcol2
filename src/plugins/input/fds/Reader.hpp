/**
 * \file src/plugins/input/fds/Reader.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief FDS file reader
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

#ifndef FDS_READER_HPP
#define FDS_READER_HPP

#include <glob.h>
#include <map>
#include <memory>
#include <libfds.h>
#include <stdint.h>

#include "config.h"

/// Observation Domain ID (contextual information)
struct ODID {
    /// Sequence number of the next IPFIX Message
    uint32_t seq_num;
    /// Template Snapshot of the last record (only for detection of template changes)
    const fds_tsnapshot_t *tsnap;

    // Default constructor
    ODID() : seq_num(0), tsnap(nullptr) {};
};

/// Transport Session (contextual information)
struct Session {
    /// Transport Session identification
    struct ipx_session *info;
    /// Context for each Observation Domain ID (key: ODID)
    std::map<uint32_t, ODID> odids;

    // Default constructor
    Session() : info(nullptr) {}
};

/// FDS File reader
class Reader {
public:
    /**
     * @brief Instance constructor
     *
     * Open a FDS File and initialize the reader
     * @param[in] ctx  Plugin context (for log and message passing)
     * @param[in] cfg  Parsed plugin configuration
     * @param[in] path File to read
     * @throw FDS_exception in case of failure (e.g. invalid file)
     */
    Reader(ipx_ctx_t *ctx, const fds_config *cfg, const char *path);
    /**
     * @brief Instance destructor
     * @note Close the file and send "close" notifications of all Transport Sessions
     */
    ~Reader();

    /**
     * @brief Generate and send one IPFIX Message with Data Records from the file
     *
     * The IPFIX Message will contain Data Records that comes from the same
     * Transport Session and shares the same features (i.e. ODID and Export Time).
     * The generate Message will contain one or more Data Records.
     *
     * If the reader detects a new Transport Session, it will automatically create
     * its particular instances and send an "open" notification to the processing
     * pipeline.
     *
     * If one or more (Options) Templates must be defined, the function will also
     * generate and pass one or more IPFIX Messages with the (Options) Templates.
     *
     * @return #IPX_OK on success
     * @return #IPX_ERR_EOF if the are not more records in the current file
     * @throw FDS_exception in case of a failure.
     */
    int
    send_batch();

private:
    /// Plugin context (log and passing messages)
    ipx_ctx_t *m_ctx;
    /// Plugin configuration
    const fds_config *m_cfg;
    /// File handler (of the file current file)
    std::unique_ptr<fds_file_t, decltype(&fds_file_close)> m_file = {nullptr, &fds_file_close};
    /// Transport Sessions (from the current file)
    std::map<fds_file_sid_t, Session> m_sessions;

    /// Signalization of an unprocessed Data Record
    bool m_unproc = false;
    /// Content of the unprocessed Data Record
    struct fds_drec m_unproc_data;
    /// Context of the unprocessed Data Record
    struct fds_file_read_ctx m_unproc_ctx;

    struct ipx_session *
    session_from_sid(fds_file_sid_t sid);
    void
    session_open(struct ipx_session *ts);
    void
    session_close(struct ipx_session *ts);

    void
    send_templates(const struct ipx_session *ts, const fds_tsnapshot_t *tsnap,
        uint32_t odid, uint32_t exp_time, uint32_t seq_num);
    void
    send_ipfix(uint8_t *msg, const struct ipx_session *ts, uint32_t odid);

    int
    record_get(const struct fds_drec **rec, const struct fds_file_read_ctx **ctx);
};

#endif // FDS_READER_HPP