/**
 * \file src/utils/arenaallocator.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Simple arena allocator
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

#include <memory>
#include <vector>

static constexpr size_t BLOCK_SIZE = 4 * 1024 * 1024;

/**
 * \brief      A simple arena allocator.
 *
 * Provides easy allocation in continuous memory areas and the ability to deallocate all allocated
 * memory upon destruction.
 */
class ArenaAllocator {
public:
    ArenaAllocator() {}

    /**
     * \brief      Disallow copy constructor.
     */
    ArenaAllocator(const ArenaAllocator &) = delete;

    /**
     * \brief      Disallow move constructor.
     */
    ArenaAllocator(ArenaAllocator &&) = delete;

    /**
     * \brief      Allocate bytes.
     *
     * \param[in]  size  The number of bytes
     *
     * \warning    size cannot be more than BLOCK_SIZE!
     *
     * \return     Pointer to the bytes
     */
    uint8_t *
    allocate(size_t size);

private:
    std::vector<std::unique_ptr<uint8_t []>> m_blocks;

    size_t m_offset = BLOCK_SIZE;
};