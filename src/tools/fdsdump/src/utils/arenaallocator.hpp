#pragma once

#include <memory>
#include <vector>

static constexpr size_t BLOCK_SIZE = 4 * 1024 * 1024;

class ArenaAllocator {
public:
    ArenaAllocator() {}

    ArenaAllocator(const ArenaAllocator &) = delete;

    ArenaAllocator(ArenaAllocator &&) = delete;

    uint8_t *
    allocate(size_t size);

private:
    std::vector<std::unique_ptr<uint8_t []>> m_blocks;
    size_t m_offset = BLOCK_SIZE;
};