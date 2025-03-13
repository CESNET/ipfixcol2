/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Sorted record storage
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <set>

#include <lister/storageRecord.hpp>
#include <lister/storageSorter.hpp>

namespace fdsdump {
namespace lister {


/**
 * @brief Sorted storage of flow records
 */
class StorageSorted {
public:
    using Storage = std::multiset<StorageRecord, StorageSorter>;

    /**
     * @brief Create a storage for Flow Data records where the records are
     * sorted based on the @p sorter.
     *
     * @param[in] sorter   Sorter of Flow Record
     * @param[in] capacity Maximal capacity of the storage. If zero, the
     *   storage capacity is not limited.
     */
    StorageSorted(StorageSorter sorter, size_t capacity = 0);

    /**
     * @brief Insert a Flow Data record to the storage and place it based
     * on the order given by the sorter.
     *
     * If the capacity has been reached and the new record would be placed
     * (based on the sorter) after the last record in the storage, no action
     * is performed.
     *
     * @param[in] flow Flow Data record to be stored
     */
    void insert(struct Flow *flow);

    /**
     * @brief Get an iterator to the first element of the storage.
     * @return Constant iterator
     */
    Storage::iterator begin() const { return m_records.begin(); };
    /**
     * @brief Get an iterator to the element following the last element the storage.
     * @return Constant iterator
     */
    Storage::iterator end() const { return m_records.end(); };

private:
    StorageSorter m_sorter;
    Storage m_records;
    size_t m_capacity;

    void insert_single_direction(struct Flow *flow);
    void insert_storage_record(struct Flow *flow);
};

} // lister
} // fdsdump
