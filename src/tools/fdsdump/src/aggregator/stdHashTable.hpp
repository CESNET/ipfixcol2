/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Standard hash table implementation
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <aggregator/allocator.hpp>
#include <aggregator/view.hpp>

#include <cstdint>
#include <functional>
#include <vector>
#include <memory>
#include <unordered_map>

namespace fdsdump {
namespace aggregator {

/**
 * @brief A standard hash table implementation based on std::unordered_map
 *
 */
class StdHashTable {
public:
    /**
     * @brief Constructs a new instance.
     * @param[in]  key_size    Number of bytes of the key portion of the record
     * @param[in]  value_size  Number of bytes of the value portion of the record
     */
    StdHashTable(const View &view);

    /**
     * @brief Find a record corresponding to the provided key
     * @param key   The key
     * @param item  The stored record including the key
     * @return true if the record was found, false otherwise
     */
    bool
    find(uint8_t *key, uint8_t *&item);

    /**
     * @brief Find a record corresponding to the provided key or create a new
     *   one if not found.
     * @param key   The key
     * @param item  The stored record including the key
     * @return true if the record was found, false if it wasn't and a new record
     *   was created
     */
    bool
    find_or_create(uint8_t *key, uint8_t *&item);

    /**
     * @brief Access the stored records.
     * @warning
     *   If the vector is modified bu the caller in some way, the behavior of
     *   following calls to the hash table methods are undefined
     * @return Vector of the stored records
     */
    std::vector<uint8_t *> &items() { return m_items; }

private:
    using Map = std::unordered_map<uint8_t *, uint8_t *,
        std::function<std::size_t(const uint8_t *)>,
        std::function<bool(const uint8_t *, const uint8_t *)>>;

    const View& m_view;
    std::vector<uint8_t *> m_items;
    Allocator m_allocator;
    Map m_map;
};

} // aggregator
} // fdsdump
