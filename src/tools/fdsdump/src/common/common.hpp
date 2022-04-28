/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @author Lukas Hutak <hutaklukas@fit.vut.cz>
 * @brief General utility functions
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <libfds.h>

using unique_file = std::unique_ptr<fds_file_t, decltype(&fds_file_close)>;
using unique_iemgr = std::unique_ptr<fds_iemgr_t, decltype(&fds_iemgr_destroy)>;
using unique_filter = std::unique_ptr<fds_ipfix_filter, decltype(&fds_ipfix_filter_destroy)>;
using shared_iemgr = std::shared_ptr<fds_iemgr_t>;
using shared_tsnapshot = std::shared_ptr<fds_tsnapshot_t>;

/**
 * @brief Split string using a user-defined delimeter.
 *
 * If no delimeter is found, the result contains only the original string.
 * @param str String to be splitted
 * @param delimiter String delimeter
 * @return Vector of piecies.
 */
std::vector<std::string>
string_split(const std::string &str, const std::string &delimiter);

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
 * @brief Copy a specified number of bits from source to destination,
 *   remaining bits in an incomplete byte are zeroed.
 * @param dst The destination
 * @param src The source
 * @param n_bits The number of bits
 */
void
memcpy_bits(uint8_t *dst, uint8_t *src, unsigned int n_bits);
