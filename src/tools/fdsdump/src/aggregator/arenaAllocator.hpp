/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Simple arena allocator
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <common/common.hpp>
#include <memory>
#include <vector>

namespace fdsdump {
namespace aggregator {

static constexpr size_t BLOCK_SIZE = 4 * 1024 * 1024;

/**
 * @brief A simple arena allocator.
 *
 * Provides easy allocation in continuous memory areas and the ability to deallocate all allocated
 * memory upon destruction.
 */
class ArenaAllocator {
    DISABLE_COPY_AND_MOVE(ArenaAllocator)

public:
    ArenaAllocator() = default;


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

} // aggregator
} // fdsdump
