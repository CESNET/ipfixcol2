/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @author Lukas Hutak <hutaklukas@fit.vut.cz>
 * @brief General utility functions
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <3rd_party/optional.hpp>

#include <memory>
#include <string>
#include <vector>
#include <cassert>
#include <limits>
#include <type_traits>

#include <libfds.h>

namespace fdsdump {

#define DISABLE_COPY_AND_MOVE(CLASSNAME) \
    CLASSNAME ( CLASSNAME &) = delete; \
    CLASSNAME (const CLASSNAME &&) = delete; \
    CLASSNAME &operator=( CLASSNAME &) = delete; \
    CLASSNAME &operator=(const CLASSNAME &&) = delete;

using unique_file = std::unique_ptr<fds_file_t, decltype(&fds_file_close)>;
using unique_iemgr = std::unique_ptr<fds_iemgr_t, decltype(&fds_iemgr_destroy)>;
using unique_filter = std::unique_ptr<fds_ipfix_filter, decltype(&fds_ipfix_filter_destroy)>;
using shared_iemgr = std::shared_ptr<fds_iemgr_t>;
using shared_tsnapshot = std::shared_ptr<fds_tsnapshot_t>;

// std::optional implementation for C++11
template <typename T>
using Optional = tl::optional<T>;

/**
 * @brief Split string using a user-defined delimeter.
 *
 * If no delimeter is found, the result contains only the original string.
 * @param str String to be splitted
 * @param delimiter String delimeter
 * @param max_pieces The maximum number of pieces (0 = no limit)
 * @return Vector of piecies.
 */
std::vector<std::string>
string_split(
    const std::string &str,
    const std::string &delimiter,
    unsigned int max_pieces = 0);

/**
 * @brief Split string by a delimiter from right.
 * @param str The string to be splitted
 * @param delimiter String delimeter
 * @param max_pieces The maximum number of pieces (0 = no limit)
 * @return Vector of the string pieces.
 */
std::vector<std::string>
string_split_right(
    const std::string &str,
    const std::string &delimiter,
    unsigned int max_pieces = 0);

void
string_ltrim(std::string &str);

void
string_rtrim(std::string &str);

void
string_trim(std::string &str);

std::string
string_trim_copy(std::string str);


/**
 * @brief Convert a string to lowercase
 *
 * @param The string to convert
 *
 * @return The supplied string converted to lowercase
 */
std::string
string_to_lower(std::string str);

/**
 * @brief Copy a specified number of bits from source to destination,
 *   remaining bits in an incomplete byte are zeroed.
 * @param dst The destination
 * @param src The source
 * @param n_bits The number of bits
 */
void
memcpy_bits(uint8_t *dst, uint8_t *src, unsigned int n_bits);

template <typename T,
    typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value, bool>::type = true
>
Optional<T>
parse_number(const std::string &s)
{
    static_assert(std::numeric_limits<T>::min() >= std::numeric_limits<long>::min()
            && std::numeric_limits<T>::max() <= std::numeric_limits<long>::max(),
            "supplied type has range broader than long thus cannot be used with strtol");

    char *end;
    errno = 0;
    long value = std::strtol(s.c_str(), &end, 10);
    if (errno != 0
            || *end != '\0'
            || value < std::numeric_limits<T>::min()
            || value > std::numeric_limits<T>::max()) {
        return {};
    }
    return value;
}

template <typename T,
    typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value, bool>::type = true
>
Optional<T>
parse_number(const std::string &s)
{
    static_assert(std::numeric_limits<T>::min() >= std::numeric_limits<unsigned long>::min()
            && std::numeric_limits<T>::max() <= std::numeric_limits<unsigned long>::max(),
            "supplied type has range broader than unsigned long thus cannot be used with strtol");

    char *end;
    errno = 0;
    unsigned long value = std::strtoul(s.c_str(), &end, 10);
    if (errno != 0
            || *end != '\0'
            || value < std::numeric_limits<T>::min()
            || value > std::numeric_limits<T>::max()) {
        return {};
    }
    return value;
}

std::vector<std::string>
glob_files(const std::string &pattern);

std::vector<std::string>
glob_files(const std::vector<std::string> &patterns);

} // fdsdump
