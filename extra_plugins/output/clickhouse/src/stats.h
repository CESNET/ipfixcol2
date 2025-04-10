/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Statistics tracking and reporting
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "common.h"
#include <cstdint>
#include <ctime>

class Plugin; // Forward declaration of Plugin class.

/**
 * @class Stats
 * @brief A class for tracking and reporting statistics related to data processing.
 *
 * The Stats class is responsible for maintaining counters and timestamps
 * to track the progress of data processing and periodically printing
 * statistics in a throttled manner.
 */
class Stats {
public:
    /**
     * @brief Constructs a Stats object.
     *
     * @param logger A Logger instance for logging statistics.
     * @param plugin A reference to the Plugin instance associated with this Stats object.
     */
    Stats(Logger logger, Plugin& plugin);

    /**
     * @brief Adds a specified number of records to the processed count.
     *
     * @param count The number of records to add.
     */
    void add_recs(uint64_t count);

    /**
     * @brief Adds a specified number of rows to the written count.
     *
     * @param count The number of rows to add.
     */
    void add_rows(uint64_t count);

    /**
     * @brief Adds a specified number of records to the dropped count.
     *
     * @param count The number of records to add.
     */
    void add_dropped(uint64_t count);

    /**
     * @brief Prints the statistics if sufficient time has passed since the last print.
     */
    void print_stats_throttled(time_t now);

private:
    Logger m_logger; ///< Logger instance for logging statistics.
    Plugin &m_plugin; ///< Reference to the associated Plugin instance.
    uint64_t m_rows_written_total = 0; ///< Total number of rows written.
    uint64_t m_recs_processed_total = 0; ///< Total number of records processed.
    uint64_t m_recs_processed_since_last = 0; ///< Records processed since the last statistics print.
    uint64_t m_recs_dropped_total = 0; ///< Total number of records dropped.
    time_t m_start_time = 0; ///< Start time of the statistics tracking.
    time_t m_last_stats_print_time = 0; ///< Time of the last statistics print.
};
