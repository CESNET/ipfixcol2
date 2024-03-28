/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Vector of bytes compatible with C allocation (source file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ByteVector.hpp"

#include <memory>    // unique_ptr
#include <cstdlib>   // realloc, size_t
#include <cstdint>   // uint8_t
#include <stdexcept> // runtime_error
#include <string>    // to_string

namespace tcp_in {

ByteVector::ByteVector(ByteVector &&other) :
    m_data(std::move(other.m_data)),
    m_size(other.m_size),
    m_capacity(other.m_capacity)
{
    // make the other valid empty vector
    other.take();
}

void ByteVector::resize(size_t new_size) {
    reserve(new_size);
    m_size = new_size;
}

void ByteVector::reserve(size_t size) {
    if (size <= m_capacity) {
        return;
    }

    auto ptr = m_data.get();
    std::unique_ptr<uint8_t [], FreeDestructor> new_ptr(
        reinterpret_cast<uint8_t *>(realloc(ptr, size))
    );

    if (!new_ptr) {
        throw std::runtime_error("Failed to reallocate ByteVector to size " + std::to_string(size));
    }

    m_data.swap(new_ptr);
    m_capacity = size;

    // the data is alread freed by realloc, don't free it again
    new_ptr.release();
}

uint8_t *ByteVector::take() noexcept {
    m_capacity = 0;
    m_size = 0;
    return m_data.release();
}

} // namespace tcp_in

