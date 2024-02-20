/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Efficient hash table implementation
 */

#define XXH_INLINE_ALL

#include "stdHashTable.hpp"
#include <cstring>
#include "3rd_party/xxhash/xxhash.h"

namespace fdsdump {
namespace aggregator {

StdHashTable::StdHashTable(std::size_t key_size, std::size_t value_size) :
    m_key_size(key_size),
    m_value_size(value_size)
{
    auto hash = [=](const uint8_t *key) {
        return XXH3_64bits(key, m_key_size);
    };
    auto equals = [=](const uint8_t *a, const uint8_t *b) {
        return std::memcmp(a, b, m_key_size) == 0;
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
        uint8_t *data = m_allocator.allocate(m_key_size + m_value_size);
        std::memcpy(data, key, m_key_size);
        m_map.insert({data, data});
        m_items.push_back(data);
        item = data;
        return false;
    }
}

} // aggregator
} // fdsdump
