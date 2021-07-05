#include "aggregate_table.hpp"

#define XXH_INLINE_ALL

#include <xmmintrin.h>
#include "xxhash.h"
#include <iostream>

AggregateTable::AggregateTable(std::size_t key_size, std::size_t value_size) :
    m_key_size(key_size), m_value_size(value_size)
{
    init_blocks();
}

void
AggregateTable::init_blocks()
{
    AggregateTableBlock zeroed_block;
    memset(&zeroed_block, 0, sizeof(zeroed_block));
    for (int i = 0; i < 16; i++) {
        zeroed_block.tags[i] = 0x80;
    }
    m_blocks.resize(m_block_count);
    for (auto &block : m_blocks) {
        block = zeroed_block;
    }
}

int
AggregateTable::lookup(uint8_t *key, AggregateRecord *&result)
{
    uint64_t hash = XXH3_64bits(key, m_key_size);
    uint64_t index = (hash >> 7) & (m_block_count - 1);
    //index = 42;
    for (;;) {
        //std::cout << "Index=" << index << std::endl;

        AggregateTableBlock &block = m_blocks[index];

        uint8_t item_tag = (hash & 0xFF) & 0x7F;
        auto block_tags = _mm_load_si128(reinterpret_cast<__m128i *>(block.tags));
        auto hash_mask = _mm_set1_epi8(item_tag);
        auto empty_mask = _mm_set1_epi8(0x80);

        auto hash_match = _mm_movemask_epi8(_mm_cmpeq_epi8(block_tags, hash_mask));
        auto empty_match = _mm_movemask_epi8(_mm_cmpeq_epi8(block_tags, empty_mask));

        int item_index = 0;  
        while (hash_match) {
            //std::cout << "Hash match=" << hash_match << std::endl;
            auto one_index = __builtin_ctz(hash_match);
            item_index += one_index;
            //std::cout << "One index=" << one_index << " Item index=" << item_index << std::endl;
            
            AggregateRecord *record = block.items[item_index];
            if (memcmp(record->data, key, m_key_size) == 0) {
                //std::cout << "Found key on " << index << ":" << item_index << std::endl;
                result = record;
                return 1;
            }
            
            hash_match >>= one_index + 1;
        }

        if (empty_match) {
            //std::cout << "Empty match=" << empty_match << std::endl;
            auto empty_index = __builtin_ctz(empty_match);
            //std::cout << "Found empty index on " << index << ":" << empty_index << std::endl;
            block.tags[empty_index] = item_tag;
            
            AggregateRecord *record = static_cast<AggregateRecord *>(calloc(1, sizeof(AggregateRecord) + m_key_size + m_value_size));
            block.items[empty_index] = record;
            m_items.push_back(record);
            m_record_count++;

            //std::cout << "Calloc done" << std::endl;
            memcpy(record->data, key, m_key_size);
            //std::cout << "Memcpy done" << std::endl;
            result = record;
            
            if (double{m_record_count} / (16 * double{m_block_count}) > 0.9) {
                expand();
            }

            return 0;
        }

        index = (index + 1) & (m_block_count - 1);
    }
}

void 
AggregateTable::expand()
{
    m_block_count *= 2;
    //std::cout << "Expand to " << m_block_count << std::endl;
    init_blocks();
    for (AggregateRecord *item : m_items) {
        uint64_t hash = XXH3_64bits(item->data, m_key_size);
        uint64_t index = (hash >> 7) & (m_block_count - 1);
        uint8_t item_tag = (hash & 0xFF) & 0x7F;

        for (;;) {
            AggregateTableBlock &block = m_blocks[index];

            auto block_tags = _mm_load_si128(reinterpret_cast<__m128i *>(block.tags));
            auto empty_mask = _mm_set1_epi8(0x80);
            auto empty_match = _mm_movemask_epi8(_mm_cmpeq_epi8(block_tags, empty_mask));
            if (empty_match) {
                //std::cout << "Empty match=" << empty_match << std::endl;
                auto empty_index = __builtin_ctz(empty_match);
                //std::cout << "Found empty index on " << index << ":" << empty_index << std::endl;
                block.tags[empty_index] = item_tag;
                block.items[empty_index] = item;
                break;
            }

            index = (index + 1) & (m_block_count - 1);
        }

    }
}