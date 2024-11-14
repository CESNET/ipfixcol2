/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Common functionality and general utilities
 * @date 2025
 *
 * Copyright(c) 2025 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include "clickhouse.h"

#include <fmt/core.h>
#include <ipfixcol2.h>

#include <stdexcept>

/**
 * @class Nonmoveable
 * @brief Classes that will inherit from this class will be immovable
 */
struct Nonmoveable {
    Nonmoveable() = default;
    Nonmoveable(Nonmoveable&&) = delete;
    Nonmoveable& operator=(Nonmoveable&&) = delete;
};

/**
 * @class Noncopyable
 * @brief Classes that will inherit from this class will not be able to be copied
 */
struct Noncopyable {
    Noncopyable() = default;
    Noncopyable(const Noncopyable&) = delete;
    Noncopyable& operator=(const Noncopyable&) = delete;
};

/**
 * @class Error
 * @brief An exception type that will be thrown by the plugin
 */
class Error : public std::runtime_error {
public:
    template <typename ...Args>
    Error(Args&& ...args) : std::runtime_error(fmt::format(args...)) {}
};


class Logger {
public:
    Logger(const ipx_ctx_t *ctx) : m_ctx(ctx) {}
    template <typename ...Args> void info   (const char *fmt, Args... args) { IPX_CTX_INFO   (m_ctx, fmt, args..., 0); }
    template <typename ...Args> void warning(const char *fmt, Args... args) { IPX_CTX_WARNING(m_ctx, fmt, args..., 0); }
    template <typename ...Args> void error  (const char *fmt, Args... args) { IPX_CTX_ERROR  (m_ctx, fmt, args..., 0); }
    template <typename ...Args> void debug  (const char *fmt, Args... args) { IPX_CTX_DEBUG  (m_ctx, fmt, args..., 0); }

private:
    const ipx_ctx_t *m_ctx;
};


/**
 * @brief Convert string to lowercase
 *
 * @param str The string
 */
std::string to_lower(std::string s);
