/**
 * \file src/utils/hashtable.hpp
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
#pragma once

#include <cstdint>
#include <vector>

#include "utils/arenaallocator.hpp"

/**
 * \brief      A struct representing a hash table block.
 */
struct HashTableBlock
{
    alignas(16) uint8_t tags[16];
    uint8_t *items[16];
};

/**
 * \brief      An efficient hash table implementation inspired by a family of hash tables known as "Swiss tables".
 */
class HashTable
{
public:
    /**
     * \brief      Constructs a new instance.
     *
     * \param[in]  key_size    Number of bytes of the key portion of the record
     * \param[in]  value_size  Number of bytes of the value portion of the record
     */
    HashTable(std::size_t key_size, std::size_t value_size);
    
    /**
     * \brief      Find a record corresponding to the provided key
     *
     * \param      key   The key
     * \param      item  The stored record including the key
     *
     * \return     true if the record was found, false otherwise
     */
    bool
    find(uint8_t *key, uint8_t *&item);


    /**
     * \brief      Find a record corresponding to the provided key or create a new one if not found
     *
     * \param      key   The key
     * \param      item  The stored record including the key
     *
     * \return     true if the record was found, false if it wasn't and a new record was created
     */
    bool
    find_or_create(uint8_t *key, uint8_t *&item);

    /**
     * \brief      Access the stored records
     *
     * \warning    If the vector is modified bu the caller in some way, the behavior of following
     *             calls to the hash table methods are undefined
     *
     * \return     Vector of the stored records
     */
    std::vector<uint8_t *> &items() { return m_items; }

private:
    std::size_t m_block_count = 4096;
    std::size_t m_record_count = 0;
    std::size_t m_key_size;
    std::size_t m_value_size;

    std::vector<HashTableBlock> m_blocks;
    std::vector<uint8_t *> m_items;

    ArenaAllocator m_allocator;

    bool
    lookup(uint8_t *key, uint8_t *&item, bool create_if_not_found);

    void
    init_blocks();

    void
    expand();
};