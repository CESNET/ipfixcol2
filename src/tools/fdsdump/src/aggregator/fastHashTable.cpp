/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Efficient hash table implementation
 */

#ifdef __SSE2__

#define XXH_INLINE_ALL

#include <xmmintrin.h>

#include <3rd_party/xxhash/xxhash.h>
#include <aggregator/fastHashTable.hpp>

namespace fdsdump {
namespace aggregator {

static constexpr double EXPAND_WHEN_THIS_FULL = 0.95;
static constexpr unsigned int EXPAND_WITH_FACTOR_OF = 2;
static constexpr uint8_t EMPTY_BIT = 0x80;

FastHashTable::FastHashTable(const View &view) :
    m_view(view)
{
    init_blocks();
}

void
FastHashTable::init_blocks()
{
    HashTableBlock zeroed_block;

    memset(&zeroed_block, 0, sizeof(zeroed_block));
    for (int i = 0; i < 16; i++) {
        zeroed_block.tags[i] |= EMPTY_BIT; // Indicate that the spot is empty
    }

    m_blocks.resize(m_block_count);
    for (auto &block : m_blocks) {
        block = zeroed_block;
    }
}

bool
FastHashTable::lookup(uint8_t *key, uint8_t *&item, bool create_if_not_found)
{
    auto key_size = m_view.key_size(key);
    uint64_t hash = XXH3_64bits(key, key_size); // The hash of the key
    uint64_t index = (hash >> 7) & (m_block_count - 1); // The starting block index

    for (;;) {
        HashTableBlock &block = m_blocks[index];

        uint8_t item_tag = (hash & 0xFF) & ~EMPTY_BIT; // Get item tag from part of the hash with the empty bit cleared
        auto block_tags = _mm_load_si128(reinterpret_cast<__m128i *>(block.tags)); // Load the current block metadata (16B)
        auto hash_mask = _mm_set1_epi8(item_tag); // Repeat the item tag 16 times
        auto empty_mask = _mm_set1_epi8(EMPTY_BIT); // Repeat the empty tag 16 times

        auto hash_match = _mm_movemask_epi8(_mm_cmpeq_epi8(block_tags, hash_mask)); // Check if any of the metadata matched our item tag
        auto empty_match = _mm_movemask_epi8(_mm_cmpeq_epi8(block_tags, empty_mask)); // Check if any of the metadata matched an empty spot

        int item_index = 0;

        while (hash_match) { // While there are any set bits indicating that the tag of the item we're looking for matched
            auto one_index = __builtin_ctz(hash_match); // Get index of the set bit, i.e. the index of the item in the block
            item_index += one_index;

            uint8_t *record = block.items[item_index]; // The record whose item tag matched
            if (m_view.key_size(record) == key_size && memcmp(record, key, key_size) == 0) { // Does the key match as well or was it just a hash collision?
                item = record;
                return true; // We found the item
            }

            // Move on to the next set bit
            hash_match >>= one_index + 1;
            item_index += 1;
        }

        // If we got here we didn't match, but we found an empty spot in the block which
        // indicates that we're done with the search. The item cannot be in the next block
        // if the current block contains an empty spot
        if (empty_match) {

            if (!create_if_not_found) {
                // If we're just looking for the item and we haven't found it, we're done
                return false;
            }

            // Create a new record
            auto empty_index = __builtin_ctz(empty_match);
            block.tags[empty_index] = item_tag;

            uint8_t *record = m_allocator.allocate(key_size + m_view.value_size());
            block.items[empty_index] = record;
            m_items.push_back(record);
            m_record_count++;

            memcpy(record, key, key_size); // Copy the key, leave the value part uninitialized
            item = record;

            // If the hash table has reached a specified percentage of fullness, expand the hash table
            if (double(m_record_count) / (16 * double(m_block_count)) >= EXPAND_WHEN_THIS_FULL) {
                expand();
            }

            return false;
        }

        index = (index + 1) & (m_block_count - 1); // Move on to the next block
    }
}

void
FastHashTable::expand()
{
    // Grow the amount of blocks by a specified factor
    m_block_count *= EXPAND_WITH_FACTOR_OF;

    // Reinitialize the blocks
    init_blocks();

    // Reassign all the items to the newly initialized blocks
    for (uint8_t *item : m_items) {
        auto key_size = m_view.key_size(item);
        uint64_t hash = XXH3_64bits(item, key_size);
        uint64_t index = (hash >> 7) & (m_block_count - 1);
        uint8_t item_tag = (hash & 0xFF) & ~EMPTY_BIT;

        // Find a spot for the item and insert it
        for (;;) {
            HashTableBlock &block = m_blocks[index];

            auto block_tags = _mm_load_si128(reinterpret_cast<__m128i *>(block.tags));
            auto empty_mask = _mm_set1_epi8(EMPTY_BIT);
            auto empty_match = _mm_movemask_epi8(_mm_cmpeq_epi8(block_tags, empty_mask));
            if (empty_match) { // Does this black have an empty spot for our item?
                auto empty_index = __builtin_ctz(empty_match);
                block.tags[empty_index] = item_tag;
                block.items[empty_index] = item;
                break;
            }

            index = (index + 1) & (m_block_count - 1);
        }

    }
}

bool
FastHashTable::find(uint8_t *key, uint8_t *&item)
{
    return lookup(key, item, false);
}

bool
FastHashTable::find_or_create(uint8_t *key, uint8_t *&item)
{
    return lookup(key, item, true);
}

} // aggregator
} // fdsdump

#endif // ifdef __SSE2__
