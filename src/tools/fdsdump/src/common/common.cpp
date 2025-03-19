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
#include <memory>
#include <stdexcept>

#include <glob.h>

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

std::vector<std::string>
glob_files(const std::string &pattern)
{
    glob_t globbuf = {};
    const int flags = GLOB_MARK | GLOB_BRACE | GLOB_TILDE;
    int ret = glob(pattern.c_str(), flags, NULL, &globbuf);
    switch (ret) {
    case 0:
        break;
    case GLOB_NOMATCH:
        return {};
    case GLOB_NOSPACE:
        throw std::bad_alloc();
    case GLOB_ABORTED:
        throw std::runtime_error("glob() failed: GLOB_ABORTED");
    default:
        throw std::runtime_error("glob() failed: " + std::to_string(ret));
    }

    std::unique_ptr<glob_t, decltype(&globfree)> glob_ptr{&globbuf, &globfree};
    std::vector<std::string> files;

    for (size_t i = 0; i < globbuf.gl_pathc; i++) {
        std::string filename = globbuf.gl_pathv[i];

        if (filename.back() == '/') {
            // Skip directories
            continue;
        }

        files.push_back(std::move(filename));
    }

    return files;
}

std::vector<std::string>
glob_files(const std::vector<std::string> &patterns)
{
    std::vector<std::string> files;
    for (const auto &pattern : patterns) {
        for (const auto &file : glob_files(pattern)) {
            files.push_back(file);
        }
    }
    return files;
}

} // fdsdump
