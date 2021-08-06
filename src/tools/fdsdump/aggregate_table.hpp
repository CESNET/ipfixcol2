#pragma once

#include <cstdint>
#include <vector>

struct AggregateRecord
{
    uint8_t data[1];
};

struct AggregateTableBlock
{
    alignas(16) uint8_t tags[16];
    AggregateRecord *items[16];
};

class AggregateTable
{
public:
    AggregateTable(std::size_t key_size, std::size_t value_size);
    
    int
    lookup(uint8_t *key, AggregateRecord *&record);

    std::vector<AggregateRecord *> &
    records() { return m_items; }

private:
    std::size_t m_block_count = 4096;
    std::size_t m_record_count = 0;
    std::size_t m_key_size;
    std::size_t m_value_size;

    std::vector<AggregateTableBlock> m_blocks;
    std::vector<AggregateRecord *> m_items;

    void
    init_blocks();

    void
    expand();
};