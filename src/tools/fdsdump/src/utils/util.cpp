/**
 * \file src/utils/util.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief General utility functions
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
#include "utils/util.hpp"

#include <cstring>

std::vector<std::string>
string_split(const std::string &str, const std::string &delimiter)
{
    std::vector<std::string> pieces;
    std::size_t pos = 0;
    for (;;) {
        std::size_t next_pos = str.find(delimiter, pos);
        if (next_pos == std::string::npos) {
            pieces.emplace_back(str.begin() + pos, str.end());
            break;
        }
        pieces.emplace_back(str.begin() + pos, str.begin() + next_pos);
        pos = next_pos + 1;
    }
    return pieces;
}

void
memcpy_bits(uint8_t *dst, uint8_t *src, unsigned int n_bits)
{
    unsigned int n_bytes = (n_bits + 7) / 8;
    unsigned int rem_bits = 8 - (n_bits % 8);
    memcpy(dst, src, n_bytes);
    if (rem_bits != 8) {
        dst[n_bytes - 1] &= (0xFF >> rem_bits) << rem_bits;
    }
}