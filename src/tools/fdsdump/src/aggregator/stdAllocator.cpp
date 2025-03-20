/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Simple allocator
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <aggregator/stdAllocator.hpp>

#include <cassert>

namespace fdsdump {
namespace aggregator {

uint8_t *
StdAllocator::allocate(size_t size)
{
    m_blocks.push_back(std::unique_ptr<uint8_t []>(new uint8_t[size]));
    return m_blocks.back().get();
}

} // aggregator
} // fdsdump
