/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Aggregator
 */
#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <aggregator/hashTable.hpp>

#include <libfds.h>

#include <aggregator/view.hpp>
#include <common/flowProvider.hpp>

namespace fdsdump {
namespace aggregator {

/**
 * @brief A class performing aggregation of fds data records based on a view definition.
 */
class Aggregator {
public:
    /**
     * @brief Constructs a new instance.
     * @param view_def  The view definition
     */
    Aggregator(const View &view);

    /**
     * @brief Process a data record.
     * @param flow The data record
     */
    void
    process_record(Flow &flow);

    /**
     * @brief Merge other aggregator into this one.
     * @param other The other aggregator
     */
    // void
    // merge(Aggregator &other, unsigned int max_num_items = 0);

    /**
     * @brief The underlying hash table.
     * @warning If modified from outside, behavior of further calls to process_record and
     *   merge are undefined!
     */
    HashTable m_table;

    /**
     * @brief Get the aggregated records-
     * @warning
     *   If modified from outside, behavior of further calls to process_record
     *   and merge are undefined!
     * @return Reference to the aggregated record
     */
    std::vector<uint8_t *> &items() { return m_table.items(); }

    void
    sort_items();

private:
    std::vector<uint8_t> m_key_buffer;
    const View &m_view;

    void
    aggregate(FlowContext &ctx);

    // void
    // aggregate(fds_drec &drec, ViewDirection direction, uint16_t drec_find_flags);
};

} // aggregator
} // fdsdump
