/**
 * \file
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Syslog output
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

#include <limits.h>
#include <ctime>
#include <unistd.h>
#include <inttypes.h>
#include <stdexcept>

#include <libfds/converters.h>

#include "Syslog.hpp"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX _POSIX_HOST_NAME_MAX
#endif

/** Delay between reconnection attempts (seconds)  */
#define RECONN_DELAY (5)
/** How ofter to report statistics (seconds) */
#define STATS_DELAY   (1)

static void
get_time(timespec &ts)
{
    if (clock_gettime(CLOCK_REALTIME_COARSE, &ts) != 0) {
        throw std::runtime_error("clock_gettime(CLOCK_REALTIME_COARSE) has failed");
    }
}

static std::string
get_hostname(syslog_hostname type)
{
    switch (type) {
    case syslog_hostname::NONE:
        return std::string("-");
    case syslog_hostname::LOCAL: {
        char buffer[HOST_NAME_MAX + 1];
        int ret;

        ret = gethostname(buffer, sizeof(buffer));
        if (ret < 0) {
            throw std::runtime_error("gethostname() has failed");
        }

        buffer[HOST_NAME_MAX] = '\0';
        return std::string(buffer);
        }
    }

    // Unimplemented option
    assert(false && "not implemented option");
    return "-";
}

static void
get_timestamp(const timespec &ts, char *buffer, size_t buffer_size)
{
    const int milliseconds = (ts.tv_nsec / 1000000);
    struct tm timestamp;
    size_t size_used;
    int ret;

    if (gmtime_r(&ts.tv_sec, &timestamp) == nullptr) {
        throw std::runtime_error("gmtime_r() has failed");
    }

    size_used = strftime(buffer, buffer_size, "%FT%T", &timestamp);
    if (size_used == 0) {
        throw std::runtime_error("strftime() has failed");
    }

    size_t remain_size = buffer_size - size_used;
    char *remain = buffer + size_used;

    ret = snprintf(remain, remain_size, ".%03d", milliseconds);
    if (ret < 0 || ret >= (int) remain_size) {
        throw std::runtime_error("snprintf() has failed");
    }

    remain_size -= ret;
    remain += ret;

    if (remain_size < 2) { // We need space for 'Z' and '\0'
        throw std::runtime_error("get_timestamp() has failed");
    }

    remain[0] = 'Z';
    remain[1] = '\0';
}

/**
 * \brief Class constructor
 * \param[in] cfg Syslog configuration
 * \param[in] ctx Instance context
 */
Syslog::Syslog(struct cfg_syslog &cfg, ipx_ctx_t *ctx)
    : Output(cfg.name, ctx)
    , m_socket(std::move(cfg.transport))
{
    timespec now;

    m_connection_time.tv_sec = 0;
    m_connection_time.tv_nsec = 0;
    m_is_stream = (m_socket->type() == SyslogType::STREAM);
    m_cnt_sent = 0;
    m_cnt_dropped = 0;

    prepare_hdr(cfg);
    get_time(now);
    connect(now);

    m_stats_time = now;
}

/** Destructor */
Syslog::~Syslog()
{
}

int
Syslog::process(const char *str, size_t len)
{
    timespec now;
    int ret;

    get_time(now);
    report_stats(now);

    if (!m_socket->is_ready()) {
        // Not connected -> try to reconnect
        ret = connect(now);
        if (ret != IPX_READY) {
            // Just ignore the record and reconnect later
            ++m_cnt_dropped;
            return IPX_OK;
        }
    }

    ret = send(now, str, len);
    if (ret < 0) {
        std::string description = m_socket->description();
        const char *err_str;
        ipx_strerror(-ret, err_str);

        IPX_CTX_ERROR(
            _ctx,
            "Connection to '%s' has failed: %s (%d)",
            description.c_str(),
            err_str,
            -ret);
    } else if (ret == 0) {
        ++m_cnt_dropped;
    } else if (ret > 0) {
        ++m_cnt_sent;
    }

    return IPX_OK;
}

void
Syslog::prepare_hdr(const struct cfg_syslog &cfg)
{
    const int priority = (cfg.priority.facility * 8) + cfg.priority.severity;

    m_hdr_prio.clear();
    m_hdr_prio += "<";
    m_hdr_prio += std::to_string(priority);
    m_hdr_prio += ">1 ";

    m_hdr_rest.clear();
    m_hdr_rest += " ";
    m_hdr_rest += get_hostname(cfg.hostname);
    m_hdr_rest += " ";
    m_hdr_rest += (!cfg.program.empty()) ? cfg.program : "-";
    m_hdr_rest += " ";
    m_hdr_rest += (cfg.proc_id) ? std::to_string(getpid()) : "-";
    m_hdr_rest += " - -";             // No MSGID and no structured data
    m_hdr_rest += " \xEF\xBB\xBF"; // "BOM" (for UTF-8 string)
}

int
Syslog::connect(const timespec &now)
{
    const std::string description = m_socket->description();
    int ret;

    // Try only one reconnection per second
    if (m_connection_time.tv_sec + RECONN_DELAY > now.tv_sec) {
        return IPX_ERR_LIMIT;
    }

    m_connection_time = now;

    ret = m_socket->open();
    if (ret < 0) {
        const char *err_str;
        ipx_strerror(-ret, err_str);
        IPX_CTX_WARNING(
            _ctx,
            "(Syslog output) Unable to connect to '%s': %s. Trying again in %d seconds.",
            description.c_str(),
            err_str,
            int(RECONN_DELAY));
        return IPX_OK;
    }

    IPX_CTX_INFO(_ctx, "(Syslog output) Connected to '%s'.", description.c_str());
    return IPX_READY;
}

int
Syslog::send(const timespec &now, const char *str, size_t len)
{
    char timestamp[128];
    char length[32];
    struct msghdr msg;
    struct iovec iovec[8];
    int iovec_idx = 0;

    memset(&msg, 0, sizeof(msg));
    get_timestamp(now, timestamp, sizeof(timestamp));

    if (m_is_stream) {
        // Add syslog message length before syslog header (will be filled later)
        iovec[iovec_idx].iov_base = length;
        iovec_idx++;
    }

    iovec[iovec_idx].iov_base = (void *) m_hdr_prio.data();
    iovec[iovec_idx].iov_len = m_hdr_prio.size();
    iovec_idx++;

    iovec[iovec_idx].iov_base = timestamp;
    iovec[iovec_idx].iov_len = strlen(timestamp);
    iovec_idx++;

    iovec[iovec_idx].iov_base = (void *) m_hdr_rest.data();
    iovec[iovec_idx].iov_len = m_hdr_rest.size();
    iovec_idx++;

    iovec[iovec_idx].iov_base = (void *) str;
    iovec[iovec_idx].iov_len = len;
    iovec_idx++;

    if (m_is_stream) {
        // Fill real syslog message length
        size_t length_size;
        uint32_t sum = 0;

        for (int i = 1; i < iovec_idx; ++i) {
            sum += iovec[i].iov_len;
        }

        // Convert number to string using very fast libfds function
        sum = htonl(sum);
        if (fds_uint2str_be(&sum, sizeof(sum), length, sizeof(length)) < 0) {
            throw "fds_uint2str_be() has failed";
        }

        length_size = strlen(length);
        length[length_size++] = ' ';
        iovec[0].iov_len = length_size;
    }

    msg.msg_iov = iovec;
    msg.msg_iovlen = iovec_idx;

    return m_socket->write(&msg);
}

void
Syslog::report_stats(const timespec &now)
{
    if (m_stats_time.tv_sec + STATS_DELAY > now.tv_sec) {
        return;
    }

    m_stats_time = now;

    IPX_CTX_INFO(
        _ctx,
        "STATS: sent: %" PRIu64 ", dropped: %" PRIu64,
        m_cnt_sent,
        m_cnt_dropped);

    m_cnt_sent = 0;
    m_cnt_dropped = 0;
}
