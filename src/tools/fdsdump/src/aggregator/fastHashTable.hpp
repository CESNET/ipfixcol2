/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Efficient hash table implementation
 */
#pragma once

#ifdef __SSE2__

#include <aggregator/allocator.hpp>
#include <aggregator/view.hpp>

#include <cstdint>
#include <vector>

namespace fdsdump {
namespace aggregator {

/**
 * @brief A struct representing a hash table block.
 */
struct HashTableBlock
{
    alignas(16) uint8_t tags[16];
    uint8_t *items[16];
};

/**
 * @brief An efficient hash table implementation inspired by a family of hash
 * tables known as "Swiss tables".
 */
class FastHashTable {
public:
    /**
     * @brief Constructs a new instance.
     * @param[in]  key_size    Number of bytes of the key portion of the record
     * @param[in]  value_size  Number of bytes of the value portion of the record
     */
    FastHashTable(const View &view);

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
    std::size_t m_block_count = 4096;
    std::size_t m_record_count = 0;
    const View &m_view;

    std::vector<HashTableBlock> m_blocks;
    std::vector<uint8_t *> m_items;

    Allocator m_allocator;

    bool
    lookup(uint8_t *key, uint8_t *&item, bool create_if_not_found);

    void
    init_blocks();

    void
    expand();
};

} // aggregator
} // fdsdump

#endif // ifdef __SSE2__ 
