/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Statistics tracking and reporting
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stats.h"
#include "plugin.h"

static constexpr int STATS_PRINT_INTERVAL_SECS = 1;

Stats::Stats(Logger logger, Plugin& plugin) : m_logger(logger), m_plugin(plugin) {}

void Stats::add_recs(uint64_t count)
{
    m_recs_processed_since_last += count;
    m_recs_processed_total += count;
}

void Stats::add_rows(uint64_t count)
{
    m_rows_written_total += count;
}

void Stats::add_dropped(uint64_t count)
{
    m_recs_dropped_total += count;
}

void Stats::print_stats_throttled(time_t now)
{
    if (m_start_time == 0) {
        m_start_time = now;
    }

    if ((now - m_last_stats_print_time) > STATS_PRINT_INTERVAL_SECS) {
        double total_rps = m_recs_processed_total / std::max<double>(1, now - m_start_time);
        double immediate_rps = m_recs_processed_since_last / std::max<double>(1, now - m_last_stats_print_time);
        m_logger.info("STATS - RECS: %lu (%lu dropped), ROWS: %lu, AVG: %.2f recs/sec, AVG_IMMEDIATE: %.2f recs/sec, BLK_AVAIL_Q: %lu, BLK_FILL_Q: %lu",
                      m_recs_processed_total,
                      m_recs_dropped_total,
                      m_rows_written_total,
                      total_rps,
                      immediate_rps,
                      m_plugin.m_avail_blocks.size(),
                      m_plugin.m_filled_blocks.size()
                      );
        m_recs_processed_since_last = 0;
        m_last_stats_print_time = now;
    }
}
