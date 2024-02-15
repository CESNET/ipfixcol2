/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief View factory
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <aggregator/field.hpp>
#include <aggregator/view.hpp>

namespace fdsdump {
namespace aggregator {

/**
 * @brief A factory class for creating aggregator fields and views
 */
class ViewFactory {
public:
    /**
     * @brief Create a field from the string definition
     *
     * @param def  The definition of the field as would be specified on the command line
     */
    static std::unique_ptr<Field> create_key_field(const std::string &def);

    /**
     * @brief Create an aggregated field from the string definition
     *
     * @param def  The definition of the field as would be specified on the command line
     */
    static std::unique_ptr<Field> create_value_field(const std::string &def);

    /**
     * @brief Create an aggregation view
     *
     * @param key_def  The definition of the key fields as specified on the command line
     * @param value_def  The definition of the value fields as specified on the command line
     * @param order_def  The definition of the order-by fields as specified on the command line
     * @param output_limit  The number of items that will be outputted using this view (0 = no limit)
     *
     * @return The view
     */
    static View create_view(
        const std::string &key_def,
        const std::string &value_def,
        const std::string &order_def,
        unsigned int output_limit = 0);

    /**
     * @brief Create an unique aggregation view
     *
     * @param key_def  The definition of the key fields as specified on the command line
     * @param value_def  The definition of the value fields as specified on the command line
     * @param order_def  The definition of the order-by fields as specified on the command line
     * @param output_limit  The number of items that will be outputted using this view (0 = no limit)
     *
     * @return The view
     */
    std::unique_ptr<View>
    static create_unique_view(
        const std::string &key_def,
        const std::string &value_def,
        const std::string &order_def,
        unsigned int output_limit);

private:
    static std::unique_ptr<Field> create_elem_or_alias(const std::string &def);

    static std::unique_ptr<Field> parse_timewindow_func(const std::string &def);

    static std::unique_ptr<Field> parse_prefixlen_field(const std::string &def);

    static std::string parse_inout_prefix(std::string &def);

    static std::unique_ptr<Field> parse_dir_field(const std::string &def);

    static void create_view(View &view, const std::string &key_def, const std::string &value_def, const std::string &order_def, unsigned int output_limit);
};

} // namespace aggregator
} // namespace fdsdump
