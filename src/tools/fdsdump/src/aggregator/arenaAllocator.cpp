/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Simple arena allocator
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <aggregator/arenaAllocator.hpp>

#include <cassert>

namespace fdsdump {
namespace aggregator {

uint8_t *
ArenaAllocator::allocate(size_t size)
{
    assert(size <= BLOCK_SIZE);

    if (BLOCK_SIZE - m_offset < size) {
        m_offset = 0;
        m_blocks.push_back(std::unique_ptr<uint8_t []>(new uint8_t[BLOCK_SIZE]));
    }

    uint8_t *mem = m_blocks.back().get() + m_offset;
    m_offset += size;
    return mem;
}

} // aggregator
} // fdsdump
