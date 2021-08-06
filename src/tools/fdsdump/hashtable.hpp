#pragma once

#include <cstdint>
#include <vector>

struct HashTableBlock
{
    alignas(16) uint8_t tags[16];
    uint8_t *items[16];
};

class HashTable
{
public:
    HashTable(std::size_t key_size, std::size_t value_size);
    
    bool
    find(uint8_t *key, uint8_t *&item);

    bool
    find_or_create(uint8_t *key, uint8_t *&item);

private:
    std::size_t m_block_count = 4096;
    std::size_t m_record_count = 0;
    std::size_t m_key_size;
    std::size_t m_value_size;

    std::vector<HashTableBlock> m_blocks;
    std::vector<uint8_t *> m_items;

    bool
    lookup(uint8_t *key, uint8_t *&item, bool create_if_not_found);

    void
    init_blocks();

    void
    expand();
};