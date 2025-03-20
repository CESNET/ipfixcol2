/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Command line options
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <stdexcept>

#include <common/logger.hpp>

namespace fdsdump {

class OptionsException : public std::invalid_argument {
public:
    OptionsException(const std::string &what_arg)
        : std::invalid_argument(what_arg)
        {};

    OptionsException(const char *what_arg)
        : std::invalid_argument(what_arg)
        {};
};

/**
 * @brief Command line arguments.
 *
 * Parse and validate user-specified command line arguments.
 */
class Options {
public:
    enum class Mode {
        undefined,
        list,
        aggregate,
        stats,
    };

    /**
     * @brief Print the usage message
     */
    static void print_usage();

    /**
     * @brief Create options with default values.
     */
    Options();
    /**
     * @brief Create options and parse command line arguments.
     * @param[in] argc Number of arguments
     * @param[in] argv Array of arguments
     */
    Options(int argc, char *argv[]);
    ~Options() = default;

    /**
     * @brief Reset all values to default.
     */
    void reset();

    bool get_help_flag() const { return m_help_flag; };

    const Mode &get_mode() const { return m_mode; };

    /** @brief Get list of provided file patterns to process.             */
    const std::vector<std::string> &get_input_file_patterns() const { return m_input_file_patterns; };
    /** @brief Get input flow filter.                    */
    const std::string &get_input_filter() const { return m_input_filter; };

    /** @brief Get number of records to print on output. */
    size_t get_output_limit() const { return m_output_limit; };
    /** @brief Get output format specifier.              */
    const std::string &get_output_specifier() const { return m_output_specifier; };

    /** @brief Get aggregation keys */
    const std::string &get_aggregation_keys() const { return m_aggregation_keys; };
    /** @brief Get aggregation values */
    const std::string &get_aggregation_values() const { return m_aggregation_values; };

    /** @brief Whether to ignore biflow direction with zero bytes and packets counter */
    bool get_biflow_autoignore() const { return m_biflow_autoignore; };

    /** @brief Get output order specificiation           */
    const std::string &get_order_by() const { return m_order_by; };

    /** @brief Get the logging level */
    LogLevel get_log_level() const { return m_log_level; }

    /** @brief Get the number of threads to use */
    unsigned int get_num_threads() const { return m_num_threads; }

private:
    Mode m_mode;

    bool m_help_flag;

    std::vector<std::string> m_input_file_patterns;
    std::string m_input_filter;

    size_t      m_output_limit;
    std::string m_output_specifier;

    std::string m_order_by;

    std::string m_aggregation_keys;
    std::string m_aggregation_values;

    bool        m_biflow_autoignore;

    LogLevel    m_log_level;

    unsigned int m_num_threads;

    void parse(int argc, char *argv[]);
    void validate();
};

} // fdsdump
