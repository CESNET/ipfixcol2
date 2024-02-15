/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Logger component
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/logger.hpp>
#include <ctime>

namespace fdsdump {

static constexpr const char *loglevel_str[] = {
    "",
    "ERROR",
    "WARN ",
    "INFO ",
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

Logger::Line::Line(LogLevel max_level, LogLevel level)
    : m_is_noop(level > max_level)
{
    if (m_is_noop)
        return;
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    char time_buf[sizeof("XX:XX:XX")] = {0};
    std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm);
    std::cerr << time_buf << " " << loglevel_str[static_cast<uint8_t>(level)] << ": ";
}

Logger::Line::~Line()
{
    if (!m_is_noop)
        std::cerr << "\n";
}

} // fdsdump
