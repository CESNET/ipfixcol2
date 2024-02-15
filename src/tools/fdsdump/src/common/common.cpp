/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Common utility functions
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/common.hpp>
#include <algorithm>


namespace fdsdump {

std::vector<std::string>
string_split(
    const std::string &str,
    const std::string &delimiter,
    unsigned int max_pieces)
{
    std::vector<std::string> pieces;
    std::size_t pos = 0;

    for (;;) {
        if (max_pieces > 0 && pieces.size() == max_pieces - 1) {
            pieces.emplace_back(str.begin() + pos, str.end());
            break;
        }

        std::size_t next_pos = str.find(delimiter, pos);
        if (next_pos == std::string::npos) {
            pieces.emplace_back(str.begin() + pos, str.end());
            break;
        }
        pieces.emplace_back(str.begin() + pos, str.begin() + next_pos);
        pos = next_pos + delimiter.size();
    }
    return pieces;
}

std::vector<std::string>
string_split_right(
    const std::string &str,
    const std::string &delimiter,
    unsigned int max_pieces)
{
    std::vector<std::string> pieces;
    std::size_t pos = str.size() - 1;

    for (;;) {
        if (max_pieces > 0 && pieces.size() == max_pieces - 1) {
            pieces.emplace(pieces.begin(), str.begin(), str.begin() + pos + 1);
            break;
        }

        std::size_t next_pos = str.rfind(delimiter, pos);
        if (next_pos == std::string::npos) {
            pieces.emplace(pieces.begin(), str.begin(), str.begin() + pos + 1);
            break;
        }
        pieces.emplace(pieces.begin(), str.begin() + next_pos + delimiter.size(), str.begin() + pos + 1);

        if (next_pos == 0) {
            pieces.emplace(pieces.begin(), "");
            break;
        }
        pos = next_pos - 1;
    }

    return pieces;
}

void
string_ltrim(std::string &str)
{
    auto is_space = [](char ch) {
        return std::isspace(int(ch));
    };

    auto first_char = std::find_if_not(str.begin(), str.end(), is_space);
    str.erase(str.begin(), first_char);
}

void
string_rtrim(std::string &str)
{
    auto is_space = [](char ch) {
        return std::isspace(int(ch));
    };

    auto last_char = std::find_if_not(str.rbegin(), str.rend(), is_space);
    str.erase(last_char.base(), str.end());
}

void
string_trim(std::string &str)
{
    string_rtrim(str);
    string_ltrim(str);
}

std::string
string_trim_copy(std::string str)
{
    string_trim(str);
    return str;
}

std::string
string_to_lower(std::string str)
{
    std::for_each(str.begin(), str.end(), [](char &c) { c = std::tolower(c); });
    return str;
}

void
memcpy_bits(uint8_t *dst, uint8_t *src, unsigned int n_bits)
{
    unsigned int n_bytes = (n_bits + 7) / 8;
    unsigned int rem_bits = 8 - (n_bits % 8);

    if (n_bits == 0)
        return;

    memcpy(dst, src, n_bytes);

    if (rem_bits != 8) {
        dst[n_bytes - 1] &= (0xFF >> rem_bits) << rem_bits;
    }
}

} // fdsdump
