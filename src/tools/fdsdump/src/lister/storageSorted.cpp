/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Sorted record storage
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cassert>
#include <iterator>

#include <lister/storageSorted.hpp>

namespace fdsdump {
namespace lister {

StorageSorted::StorageSorted(StorageSorter sorter, size_t capacity)
    : m_sorter{sorter}, m_records{m_sorter}, m_capacity{capacity}
{
}

void
StorageSorted::insert(struct Flow *flow)
{
    const enum Direction dir_backup = flow->dir;

    if ((dir_backup & DIRECTION_FWD) != 0) {
        flow->dir = DIRECTION_FWD;
        insert_single_direction(flow);
    }

    if ((dir_backup & DIRECTION_REV) != 0) {
        flow->dir = DIRECTION_REV;
        insert_single_direction(flow);
    }

    flow->dir = dir_backup;
}

void
StorageSorted::insert_single_direction(struct Flow *flow)
{
    // Exactly single direction must be specified
    assert(flow->dir == DIRECTION_FWD || flow->dir == DIRECTION_REV);

    if (m_capacity == 0 || m_records.size() < m_capacity) {
        insert_storage_record(flow);
        return;
    }

    const auto last_iter = std::prev(m_records.end());
    const Flow &last_rec = last_iter->get_flow_const();

    if (!m_sorter(*flow, last_rec)) {
        // Don't insert the record as it should be placed after the last one
        return;
    }

    m_records.erase(last_iter);
    insert_storage_record(flow);
}

void
StorageSorted::insert_storage_record(struct Flow *flow)
{
    shared_tsnapshot snapshot{
        fds_tsnapshot_deep_copy(flow->rec.snap),
        &fds_tsnapshot_destroy};

    if (!snapshot) {
        throw std::runtime_error("fds_tsnapshot_deep_copy() has failed");
    }

    m_records.emplace(flow->rec, flow->dir, snapshot);
}

} // lister
} // fdsdump
