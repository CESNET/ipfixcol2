/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Logger component
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <common/common.hpp>

#include <iostream>

namespace fdsdump {

/**
 * The logging level
 */
enum class LogLevel : uint8_t {
    none,
    error,
    warning,
    info,
    debug,
    trace
};

/**
 * @brief Increase the log level
 */
LogLevel operator++(LogLevel &level, int);

/**
 * @brief Decrease the log level
 */
LogLevel operator--(LogLevel &level, int);

/**
 * @brief A logger component
 */
class Logger {
public:
    /**
     * @brief A single log line
     */
    class Line {
        DISABLE_COPY_AND_MOVE(Line)

    public:
        /**
         * @brief Finish the log line
         */
        ~Line();

        /**
         * @brief Write value onto the log line
         *
         * @param value  The value to write
         *
         * @return reference to self
         */
        template <typename T>
        Line &operator<<(const T& value)
        {
            if (!m_ignored) {
                std::cerr << value;
            }
            return *this;
        }

    private:
        friend class Logger;

        bool m_ignored;
        Line(LogLevel verbosity_level, LogLevel message_level);
    };


    /**
     * @brief Get the instance of the logger
     */
    static Logger &get_instance();

    /**
     * @brief Set the maximum level of messages to show
     *
     * @param level  The level
     */
    void set_log_level(LogLevel level)
    {
        m_log_level = level;
    }

    /**
     * @brief Log a line on a specified logging level
     *
     * @param level  The log level
     *
     * @return The line object
     */
    Line log(LogLevel level)
    {
        return {m_log_level, level};
    }

private:
    LogLevel m_log_level = LogLevel::trace;

    Logger() = default;
};

#define LOG_TRACE (Logger::get_instance().log(LogLevel::trace))
#define LOG_DEBUG (Logger::get_instance().log(LogLevel::debug))
#define LOG_INFO (Logger::get_instance().log(LogLevel::info))
#define LOG_WARNING (Logger::get_instance().log(LogLevel::warning))
#define LOG_ERROR (Logger::get_instance().log(LogLevel::error))

} // fdsdump
