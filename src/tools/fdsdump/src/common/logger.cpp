/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Logger component
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/logger.hpp>

namespace fdsdump {

static constexpr const char *loglevel_str[] = {
    "",
    "ERROR",
    "WARNING",
    "INFO",
    "DEBUG",
    "TRACE"
};

Logger &Logger::get_instance()
{
    static Logger logger;
    return logger;
}

LogLevel operator++(LogLevel &level, int)
{
    LogLevel old = level;
    if (level != LogLevel::trace) {
        level = static_cast<LogLevel>(static_cast<uint8_t>(level) + 1);
    }
    return old;
}

LogLevel operator--(LogLevel &level, int)
{
    LogLevel old = level;
    if (level != LogLevel::none) {
        level = static_cast<LogLevel>(static_cast<uint8_t>(level) - 1);
    }
    return old;
}

Logger::Line::Line(LogLevel verbosity_level, LogLevel message_level)
    : m_ignored(message_level > verbosity_level)
{
    if (!m_ignored) {
        std::cerr << loglevel_str[static_cast<uint8_t>(message_level)] << ": ";
    }
}

Logger::Line::~Line()
{
    if (!m_ignored) {
        std::cerr << "\n";
    }
}

} // fdsdump
