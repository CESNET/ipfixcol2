/**
 * \file src/utils/hashtable.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Efficient hash table implementation
 *
 * Copyright (C) 2021 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */
#include "utils/hashtable.hpp"

#define XXH_INLINE_ALL

#include <iostream>
#include <xmmintrin.h>

#include "3rdparty/xxhash.h"

static constexpr double EXPAND_WHEN_THIS_FULL = 0.95;
static constexpr unsigned int EXPAND_WITH_FACTOR_OF = 2;

enum : uint8_t {
    EMPTY_BIT = 0x80
};

HashTable::HashTable(std::size_t key_size, std::size_t value_size) :
    m_key_size(key_size), m_value_size(value_size)
{
    init_blocks();
}

void
HashTable::init_blocks()
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
HashTable::lookup(uint8_t *key, uint8_t *&item, bool create_if_not_found)
{
    uint64_t hash = XXH3_64bits(key, m_key_size); // The hash of the key
    uint64_t index = (hash >> 7) & (m_block_count - 1); // The starting block index

    //if (m_debug) printf("Hash is %lu\n", hash);

    //index = 42;
    for (;;) {
        //std::cout << "Index=" << index << std::endl;

        HashTableBlock &block = m_blocks[index];

        uint8_t item_tag = (hash & 0xFF) & ~EMPTY_BIT; // Get item tag from part of the hash with the empty bit cleared
        auto block_tags = _mm_load_si128(reinterpret_cast<__m128i *>(block.tags)); // Load the current block metadata (16B)
        auto hash_mask = _mm_set1_epi8(item_tag); // Repeat the item tag 16 times
        auto empty_mask = _mm_set1_epi8(EMPTY_BIT); // Repeat the empty tag 16 times

        auto hash_match = _mm_movemask_epi8(_mm_cmpeq_epi8(block_tags, hash_mask)); // Check if any of the metadata matched our item tag
        auto empty_match = _mm_movemask_epi8(_mm_cmpeq_epi8(block_tags, empty_mask)); // Check if any of the metadata matched an empty spot

        int item_index = 0;

        while (hash_match) { // While there are any set bits indicating that the tag of the item we're looking for matched
            //if (m_debug) std::cout << "Hash match=" << hash_match << std::endl;
            auto one_index = __builtin_ctz(hash_match); // Get index of the set bit, i.e. the index of the item in the block
            item_index += one_index;
            //if (m_debug) std::cout << "One index=" << one_index << " Item index=" << item_index << std::endl;

            uint8_t *record = block.items[item_index]; // The record whose item tag matched
            if (memcmp(record, key, m_key_size) == 0) { // Does the key match as well or was it just a hash collision?
                //if (m_debug) std::cout << "Found key on " << index << ":" << item_index << std::endl;
                item = record;
                return true; // We found the itme
            }

            // Move on to the next set bit
            hash_match >>= one_index + 1;
            item_index += 1;
        }

        // If we got here we didn't match, but we found an empty spot in the block which indicates that we're done with the search
        // The item cannot be in the next block if the current block contains an empty spot
        if (empty_match) {

            if (!create_if_not_found) {
                // If we're just looking for the item and we haven't found it, we're done
                //if (m_debug) printf("Not found\n");
                return false;
            }

            // Create a new record

            //std::cout << "Empty match=" << empty_match << std::endl;
            auto empty_index = __builtin_ctz(empty_match);
            //std::cout << "Found empty index on " << index << ":" << empty_index << std::endl;
            block.tags[empty_index] = item_tag;

            //uint8_t *record = new uint8_t[m_key_size + m_value_size];
            uint8_t *record = m_allocator.allocate(m_key_size + m_value_size);
            block.items[empty_index] = record;
            m_items.push_back(record);
            m_record_count++;

            memcpy(record, key, m_key_size); // Copy the key, leave the value part uninitialized
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
HashTable::expand()
{
    // Grow the amount of blocks by a specified factor
    m_block_count *= EXPAND_WITH_FACTOR_OF;

    //std::cout << "Expand to " << m_block_count << std::endl;
    // Reinitialize the blocks
    init_blocks();

    // Reassign all the items to the newly initialized blocks
    for (uint8_t *item : m_items) {
        uint64_t hash = XXH3_64bits(item, m_key_size);
        uint64_t index = (hash >> 7) & (m_block_count - 1);
        uint8_t item_tag = (hash & 0xFF) & ~EMPTY_BIT;

        // Find a spot for the item and insert it
        for (;;) {
            HashTableBlock &block = m_blocks[index];

            auto block_tags = _mm_load_si128(reinterpret_cast<__m128i *>(block.tags));
            auto empty_mask = _mm_set1_epi8(EMPTY_BIT);
            auto empty_match = _mm_movemask_epi8(_mm_cmpeq_epi8(block_tags, empty_mask));
            if (empty_match) { // Does this black have an empty spot for our item?
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

bool
HashTable::find(uint8_t *key, uint8_t *&item)
{
    return lookup(key, item, false);
}

bool
HashTable::find_or_create(uint8_t *key, uint8_t *&item)
{
    return lookup(key, item, true);
}