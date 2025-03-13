/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Argument parser component
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <string>
#include <vector>

namespace fdsdump {

class Args {
    friend class ArgParser;

public:
    /**
     * @brief Check if a short option exists.
     *
     * @param short_opt The short option character to check.
     * @return true if the short option exists, false otherwise.
     */
    bool has(char short_opt) const;

    /**
     * @brief Check if a long option exists.
     *
     * @param long_opt The long option string to check.
     * @return true if the long option exists, false otherwise.
     */
    bool has(const std::string &long_opt) const;

    /**
     * @brief Check if a positional argument exists.
     *
     * @param pos The position index of the argument.
     * @return true if the positional argument exists, false otherwise.
     */
    bool has(int pos) const;

    /**
     * @brief Count the occurrences of a short option.
     *
     * @param short_opt The short option character to count.
     * @return The count of occurrences of the short option.
     */
    bool count(char short_opt) const;

    /**
     * @brief Count the occurrences of a long option.
     *
     * @param long_opt The long option string to count.
     * @return The count of occurrences of the long option.
     */
    bool count(const std::string &long_opt) const;

    /**
     * @brief Check if positional arguments exist.
     *
     * @return The count of positional arguments.
     */
    bool pos_count() const;

    /**
     * @brief Get the value of a short option.
     *
     * @param short_opt The short option character.
     * @return The value associated with the short option.
     */
    std::string get(char short_opt) const;

    /**
     * @brief Get the value of a long option.
     *
     * @param long_opt The long option string.
     * @return The value associated with the long option.
     */
    std::string get(const std::string &long_opt) const;

    /**
     * @brief Get the positional argument at a specified index.
     *
     * @param pos The position index of the argument.
     * @return The value of the positional argument.
     */
    std::string get(int pos) const;

    /**
     * @brief Get all values associated with a short option.
     *
     * @param short_opt The short option character.
     * @return A vector containing all values associated with the short option.
     */
    std::vector<std::string> get_all(char short_opt) const;

    /**
     * @brief Get all values associated with a long option.
     *
     * @param long_opt The long option string.
     * @return A vector containing all values associated with the long option.
     */
    std::vector<std::string> get_all(const std::string &long_opt) const;

private:
    struct NamedArg {
        char short_opt;
        std::string long_opt;
        std::string value;
    };

    std::vector<NamedArg> m_named_args;
    std::vector<std::string> m_pos_args;
};

class ArgParser {
public:
    struct UnknownArgument {
        std::string arg;
    };

    struct MissingArgument {
        std::string arg;
    };

    /**
     * @brief Add a new option with a short option, a long option, and indication if it requires a value.
     *
     * @param short_opt The short option character.
     * @param long_opt The long option string.
     * @param requires_value Indicates whether the option requires a value.
     */
    void add(char short_opt, const std::string &long_opt, bool requires_value);

    /**
     * @brief Add a new option with a short option and indication if it requires a value.
     *
     * @param short_opt The short option character.
     * @param requires_value Indicates whether the option requires a value.
     */
    void add(char short_opt, bool requires_value);

    /**
     * @brief Add a new option with a long option and indication if it requires a value.
     *
     * @param long_opt The long option string.
     * @param requires_value Indicates whether the option requires a value.
     */
    void add(const std::string &long_opt, bool requires_value);

    /**
     * @brief Parse the command-line arguments.
     *
     * @param argc The number of command-line arguments.
     * @param argv The array of command-line argument strings.
     * @return Args object containing parsed arguments.
     */
    Args parse(int argc, char **argv) const;

private:
    struct ArgDef {
        char short_opt;
        std::string long_opt;
        bool requires_value;
    };

    std::vector<ArgDef> m_defs;
};

} // fdsdump
