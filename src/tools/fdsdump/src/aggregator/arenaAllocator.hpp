/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Simple arena allocator
 */
#pragma once

#include <memory>
#include <vector>

static constexpr size_t BLOCK_SIZE = 4 * 1024 * 1024;

/**
 * @brief A simple arena allocator.
 *
 * Provides easy allocation in continuous memory areas and the ability to deallocate all allocated
 * memory upon destruction.
 */
class ArenaAllocator {
public:
    ArenaAllocator() {}

    /** @brief Disallow copy constructor. */
    ArenaAllocator(const ArenaAllocator &) = delete;
    /** @brief Disallow move constructor. */
    ArenaAllocator(ArenaAllocator &&) = delete;

    /**
     * @brief Allocate bytes.
     * @param[in] size  The number of bytes
     * @warning size cannot be more than BLOCK_SIZE!
     * @return Pointer to the bytes
     */
    uint8_t *allocate(size_t size);

private:
    std::vector<std::unique_ptr<uint8_t []>> m_blocks;
    size_t m_offset = BLOCK_SIZE;
};
