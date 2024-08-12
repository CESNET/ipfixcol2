/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Vector of bytes compatible with C allocation (header file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <memory>    // std::unique_ptr
#include <cstdint>   // uint8_t
#include <cstdlib>   // free, size_t
#include <algorithm> // std::swap

namespace tcp_in {

struct FreeDestructor {
    inline void operator()(void *p) noexcept {
        free(p);
    }
};

class ByteVector {
public:
    /** Creates empty vector, to allocate it use `resize`. */
    ByteVector() : m_data(), m_size(0), m_capacity(0) {};

    /**
     * @brief Move constructor, transfers the data from `other` and makes `other` empty.
     * @param other Vector to move from.
     */
    ByteVector(ByteVector &&other);

    /**
     * @brief Move assignment operator.
     * @param other Vector to move from.
     * @return Reference to this.
     */
    ByteVector &operator=(ByteVector &&other) {
        m_data.swap(other.m_data);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
        return *this;
    }

    /**
     * @brief Resizes the vector, if `new_size` is larger than current size the values of the new
     * data is undefined.
     * @param new_size new size of the vector
     * @throws std::runtime_error when reallocation fails
     */
    void resize(size_t new_size);

    /**
     * @brief Increases the capacity to `size`. Does nothing if the capacity is more than `size`.
     * @param new_size new capacity of the vector.
     * @throws std::runtime_error when reallocation fails
     */
    void reserve(size_t size);

    /**
     * @brief Releases the ownership of the data and returns pointer to it. This has the effect of
     * emptying this vector.
     * @return uint8_t*
     */
    uint8_t *take() noexcept;

    /**
     * @brief Checks whether there is some data in the vector.
     * @return true if there is no data, otherwise false.
     */
    bool is_empty() const noexcept {
        return m_size == 0;
    }

    /**
     * @brief Gets the number of used bytes in the vector.
     * @return Number of used bytes.
     */
    size_t size() const noexcept {
        return m_size;
    }

    /**
     * @brief Gets the number of bytes the vector can hold before reallocation.
     * @return Number of bytes the vector can hold before reallocation.
     */
    size_t capacity() const noexcept {
        return m_capacity;
    }

    /** Removes all data from the vector (doesn't free it). */
    void clear() noexcept {
        m_size = 0;
    }

    /**
     * @brief Gets pointer to the data. The pointer is valid until reallocation.
     * @return Pointer to the data.
     */
    uint8_t *data() noexcept {
        return m_data.get();
    }

    /**
     * @brief Gets constant pointer to the data. The pointer is valid until reallocation.
     * @return Pointer to the data.
     */
    const uint8_t *data() const noexcept {
        return m_data.get();
    }

private:
    ByteVector(uint8_t *data, size_t size, size_t capacity) noexcept :
        m_data(data),
        m_size(size),
        m_capacity(capacity)
    {}

    std::unique_ptr<uint8_t [], FreeDestructor> m_data;
    size_t m_size;
    size_t m_capacity;
};

} // namespace tcp_in

