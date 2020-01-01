/**
 * \file src/plugins/output/fds/src/Exception.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Plugin specific exception (header file)
 * \date 2019
 *
 * Copyright(c) 2019 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IPFIXCOL2_FDS_EXCEPTION_HPP
#define IPFIXCOL2_FDS_EXCEPTION_HPP

#include <stdexcept>
#include <string>

/// Plugin specific exception
class FDS_exception : public std::runtime_error {
public:
    /**
     * @brief Constructor
     * @param[in] str Error message
     */
    FDS_exception(const std::string &str) : std::runtime_error(str) {};
    /**
     * @brief Constructor
     * @param[in] str Error message
     */
    FDS_exception(const char *str) : std::runtime_error(str) {};
    // Default destructor
    ~FDS_exception() = default;
};

#endif //IPFIXCOL2_FDS_EXCEPTION_HPP
