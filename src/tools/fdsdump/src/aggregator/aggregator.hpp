/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Aggregator
 */
#pragma once

#include <array>
#include <vector>
#include <cstdint>

#include <libfds.h>

#include "common/flowProvider.hpp"
#include "hashTable.hpp"
#include "sort.hpp"

/**
 * @brief A class performing aggregation of fds data records based on a view definition.
 */
class Aggregator {
public:
    /**
     * @brief Constructs a new instance.
     * @param view_def  The view definition
     */
    Aggregator(ViewDefinition view_def);

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
    void
    merge(Aggregator &other, unsigned int max_num_items = 0);

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

private:
    ViewDefinition m_view_def;
    std::vector<uint8_t> m_key_buffer;

    void
    aggregate(fds_drec &drec, ViewDirection direction, uint16_t drec_find_flags);
};

