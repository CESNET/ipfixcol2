/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Efficient hash table implementation
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define XXH_INLINE_ALL

#include "stdHashTable.hpp"
#include <cstring>
#include "3rd_party/xxhash/xxhash.h"

namespace fdsdump {
namespace aggregator {

StdHashTable::StdHashTable(const View& view) :
    m_view(view)
{
    auto hash = [this](const uint8_t *key) {
        auto key_size = m_view.key_size(key);
        return XXH3_64bits(key, key_size);
    };
    auto equals = [this](const uint8_t *a, const uint8_t *b) {
        auto key_size = m_view.key_size(a);
        auto key_size2 = m_view.key_size(b);
        return key_size == key_size2 && std::memcmp(a, b, key_size) == 0;
    };
    m_map = Map(1, hash, equals);
}

bool
StdHashTable::find(uint8_t *key, uint8_t *&item)
{
    auto it = m_map.find(key);
    if (it != m_map.end()) {
        item = it->second;
        return true;
    } else {
        return false;
    }
}

bool
StdHashTable::find_or_create(uint8_t *key, uint8_t *&item)
{
    auto it = m_map.find(key);
    if (it != m_map.end()) {
        item = it->second;
        return true;
    } else {
        auto key_size = m_view.key_size(key);
        uint8_t *data = m_allocator.allocate(key_size + m_view.value_size());
        std::memcpy(data, key, key_size);
        m_map.insert({data, data});
        m_items.push_back(data);
        item = data;
        return false;
    }
}

} // aggregator
} // fdsdump
