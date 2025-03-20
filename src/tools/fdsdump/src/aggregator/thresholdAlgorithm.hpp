/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Threshold algorithm implementation
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <aggregator/hashTable.hpp>
#include <aggregator/view.hpp>

#include <vector>
#include <queue>
#include <limits>
#include <memory>

namespace fdsdump {
namespace aggregator {

class ThresholdAlgorithm {
public:
    std::unique_ptr<HashTable> m_result_table;

    /**
     * @brief Instantiate a new threshold algorithm instance
     *
     * @param tables  A vector of the tables containing the records that will be merged
     * @param view  The view of the record
     * @param top_count  The maximum number of values we want to obtain, i.e. the N in "top N"
     */
    ThresholdAlgorithm(std::vector<HashTable *> &tables, View &view, unsigned int top_count);

    /**
     * @brief Perform a single step of the algorithm,
     *        i.e. take a row from every table and update the result table accordingly.
     */
    void process_row();

    /**
     * @brief Check if we have run out of items to process more steps of the algorithm
     */
    bool out_of_items();

    /**
     * @brief Check if the finish condition has been reached
     */
    bool check_finish_condition();

    /**
     * @brief Set the maximum number of rows that can be processed in every table.
     *
     * @param max_row  The maximum number of rows
     */
    void set_max_row(unsigned int max_row) { m_max_row = max_row; }

private:
    using PriorityRecQueue = std::priority_queue<uint8_t *, std::vector<uint8_t *>, View::RecOrdFnType>;

    std::vector<HashTable *> &m_tables;
    View &m_view;
    unsigned int m_top_count;
    PriorityRecQueue m_min_queue;
    unsigned int m_row = 0;
    unsigned int m_max_row = std::numeric_limits<unsigned int>::max();
    std::vector<uint8_t> m_threshold;

};

} // aggregator
} // fdsdump
