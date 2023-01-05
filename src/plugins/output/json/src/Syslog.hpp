/**
 * \file
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Syslog output (header file)
 * \date 2023
 */

/* Copyright (C) 2023 CESNET, z.s.p.o.
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

#ifndef JSON_SYSLOG_H
#define JSON_SYSLOG_H

#include "Storage.hpp"
#include "SyslogSocket.hpp"

/** JSON syslog                                                                   */
class Syslog : public Output {
public:
    // Constructor
    Syslog(struct cfg_syslog &cfg, ipx_ctx_t *ctx);
    // Destructor
    ~Syslog();

    // Processing records
    int process(const char *str, size_t len);

private:
    /** Syslog socket                                                             */
    std::unique_ptr<SyslogSocket> m_socket;
    /** Time of the last connection attempt                                       */
    struct timespec m_connection_time;
    /** Identification whether connection is stream (i.e. not a datagram)         */
    bool m_is_stream;
    /** Syslog header priority (i.e. part before timestamp)                       */
    std::string m_hdr_prio;
    /** Syslog header rest (i.e. after timestamp)                                 */
    std::string m_hdr_rest;

    /** Number of sent records                                                    */
    uint64_t m_cnt_sent;
    /** Number of dropped records                                                 */
    uint64_t m_cnt_dropped;
    /** Time of the last stats report                                             */
    struct timespec m_stats_time;

    void prepare_hdr(const struct cfg_syslog &cfg);
    int connect(const timespec &now);
    int send(const timespec &now, const char *str, size_t len);
    void report_stats(const timespec &now);
};

#endif // JSON_SYSLOG_H
