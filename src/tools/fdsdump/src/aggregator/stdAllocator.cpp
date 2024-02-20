/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Simple allocator
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
