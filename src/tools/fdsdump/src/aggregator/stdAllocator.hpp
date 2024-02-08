/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Simple allocator
 */
#pragma once

#include <memory>
#include <vector>

namespace fdsdump {
namespace aggregator {

/**
 * @brief A simple allocator.
 *
 */
class StdAllocator {
public:
    StdAllocator() {}

    /** @brief Disallow copy constructor. */
    StdAllocator(const StdAllocator &) = delete;
    /** @brief Disallow move constructor. */
    StdAllocator(StdAllocator &&) = delete;

    /**
     * @brief Allocate bytes.
     * @param[in] size  The number of bytes
     * @return Pointer to the bytes
     */
    uint8_t *allocate(size_t size);

private:
    std::vector<std::unique_ptr<uint8_t []>> m_blocks;
};

} // aggregator
} // fdsdump