/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief View factory
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <aggregator/aggregatedField.hpp>
#include <aggregator/aliasField.hpp>
#include <aggregator/extraFields.hpp>
#include <aggregator/inOutField.hpp>
#include <aggregator/ipfixField.hpp>
#include <aggregator/viewFactory.hpp>
#include <common/common.hpp>
#include <common/ieMgr.hpp>

#include <limits>
#include <stdexcept>

namespace fdsdump {
namespace aggregator {

static bool
check_strip_prefix(std::string& str, const std::string &prefix)
{
    if (string_to_lower(str.substr(0, prefix.size())) == string_to_lower(prefix)) {
        str = str.substr(prefix.size());
        return true;
    }
    return false;
}

static bool
check_strip_suffix(std::string& str, const std::string &suffix)
{
    if (str.size() < suffix.size()) {
        return false;
    }

    const std::string &tail_lower = string_to_lower(
        str.substr(str.size() - suffix.size(), suffix.size()));
    const std::string &suffix_lower = string_to_lower(suffix);
    if (tail_lower == suffix_lower) {
        str = str.substr(0, str.size() - suffix.size());
        return true;
    }
    return false;
}

static Optional<int>
parse_number(const std::string &str)
{
    char *end;
    errno = 0;
    long value = std::strtol(str.c_str(), &end, 10);
    if (errno != 0
            || *end != '\0'
            || value < std::numeric_limits<int>::min()
            || value > std::numeric_limits<int>::max()) {
        return {};
    }
    return value;
}

static Optional<int>
parse_time_unit_to_millisecs(std::string str)
{
    int multiplier = 1;
    if (check_strip_suffix(str, "s")) {
        multiplier = 1;
    } else if (check_strip_suffix(str, "m")) {
        multiplier = 60;
    } else if (check_strip_suffix(str, "h")) {
        multiplier = 3600;
    }

    auto secs = parse_number(str);
    if (!secs) {
        return {};
    }

    return *secs * multiplier * 1000;
}

static std::string
parse_func_name(std::string &def)
{
    if (def.empty()) {
        return "";
    }
    if (def[def.size() - 1] != ')') {
        return "";
    }

    const auto &pieces = string_split(std::string(def.begin(), def.end() - 1), "(", 2);
    if (pieces.size() != 2) {
        return "";
    }

    def = pieces[1];
    return string_to_lower(pieces[0]);
}

std::unique_ptr<Field>
ViewFactory::create_elem_or_alias(const std::string &def)
{
    const fds_iemgr_alias *alias =
        fds_iemgr_alias_find(IEMgr::instance().ptr(), def.c_str());
    if (alias) {
        std::unique_ptr<Field> field(new AliasField(*alias));
        field->set_name(def);
        return field;
    }

    const fds_iemgr_elem *elem =
        fds_iemgr_elem_find_name(IEMgr::instance().ptr(), def.c_str());
    if (elem) {
        std::unique_ptr<Field> field(new IpfixField(*elem));
        field->set_name(def);
        return field;
    }

    throw std::invalid_argument("cannot find field \"" + def + "\"");
}

std::unique_ptr<Field>
ViewFactory::parse_timewindow_func(const std::string &def)
{
    std::string def_copy = def;
    auto func = parse_func_name(def_copy);
    if (func != "timewindow") {
        return {};
    }

    auto args = string_split(def_copy, ",");
    if (args.size() != 2) {
        throw std::runtime_error("timewindow field bad args"); //FIXME
    }

    for (auto &arg : args) {
        string_trim(arg);
    }

    auto inner = create_elem_or_alias(args[0]);
    auto millis = parse_time_unit_to_millisecs(args[1]);
    if (!millis) {
        throw std::runtime_error("timewindow field bad time unit"); //FIXME
    }

    std::unique_ptr<Field> field(new TimeWindowField(std::move(inner), *millis));
    field->set_name(def);
    return field;
}

std::unique_ptr<Field>
ViewFactory::parse_prefixlen_field(const std::string &def)
{
    auto pieces = string_split(def, "/", 2);
    if (pieces.size() != 2) {
        return {};
    }

    auto prefix_len = parse_number(pieces[1]);
    if (!prefix_len) {
        throw std::runtime_error("invalid ip prefix len"); //FIXME
    }

    auto inner = create_elem_or_alias(pieces[0]);

    std::unique_ptr<Field> field(new SubnetField(std::move(inner), *prefix_len));
    field->set_name(def);
    return field;
}

std::string
ViewFactory::parse_inout_prefix(std::string &def)
{
    std::string dir = "";
    if (check_strip_prefix(def, "in ") || check_strip_prefix(def, "in")) {
        dir = "in";
    } else if (check_strip_prefix(def, "out ") || check_strip_prefix(def, "out")) {
        dir = "out";
    }
    return dir;
}

std::unique_ptr<Field>
ViewFactory::parse_dir_field(const std::string &def)
{
    std::string def_lower = string_to_lower(def);
    if (def_lower == "direction" || def_lower == "dir" || def_lower == "biflowdir") {
        std::unique_ptr<Field> field(new DirectionField);
        field->set_name(def);
        return field;
    } else {
        return {};
    }
}

std::unique_ptr<Field>
ViewFactory::create_key_field(const std::string &def)
{
    std::unique_ptr<Field> field;

    if ((field = parse_timewindow_func(def))) {
        return field;
    }

    if ((field = parse_prefixlen_field(def))) {
        return field;
    }

    if ((field = parse_dir_field(def))) {
        return field;
    }

    field = create_elem_or_alias(def);
    return field;
}

std::unique_ptr<Field>
ViewFactory::create_value_field(const std::string& def)
{
    std::string rem_def = def;

    std::unique_ptr<Field> field;

    std::string func = parse_func_name(rem_def);
    std::string prefix = parse_inout_prefix(rem_def);

    std::string rem_def_lower = string_to_lower(rem_def);
    if (rem_def_lower == "flows" || rem_def_lower == "flowcount" || rem_def_lower == "count") {
        field.reset(new FlowCountField);
        field->set_name(rem_def);
    } else {
        field = create_elem_or_alias(rem_def);
    }

    if (func == "min") {
        field.reset(new MinAggregatedField(std::move(field)));
    } else if (func == "max") {
        field.reset(new MaxAggregatedField(std::move(field)));
    } else if (func == "sum") {
        field.reset(new SumAggregatedField(std::move(field)));
    } else if (func == "" && field->is_number()) {
        field.reset(new SumAggregatedField(std::move(field)));
    } else {
        throw std::invalid_argument("invalid aggregation function " + func);
    }

    if (prefix == "in") {
        field.reset(new InOutValueField(std::move(field), ViewDirection::In));
    } else if (prefix == "out") {
        field.reset(new InOutValueField(std::move(field), ViewDirection::Out));
    }

    field->set_name(def);
    return field;
}

static std::vector<std::string>
split_args(const std::string &str)
{
    std::vector<std::string> pieces;
    std::string tmp;
    int depth = 0;
    for (char c : str) {
        if (c == '(') {
            depth++;
            tmp.push_back(c);
        } else if (c == ')') {
            depth--;
            tmp.push_back(c);
        } else if (c == ',' && depth == 0) {
            pieces.push_back(tmp);
            tmp.clear();
        } else {
            tmp.push_back(c);
        }
    }
    pieces.push_back(tmp);
    return pieces;
}

View
ViewFactory::create_view(
    const std::string &key_def,
    const std::string &value_def,
    const std::string &order_def,
    unsigned int output_limit)
{
    View view;
    create_view(view, key_def, value_def, order_def, output_limit);
    return view;
}

std::unique_ptr<View>
ViewFactory::create_unique_view(
    const std::string &key_def,
    const std::string &value_def,
    const std::string &order_def,
    unsigned int output_limit)
{
    std::unique_ptr<View> view(new View);
    create_view(*view.get(), key_def, value_def, order_def, output_limit);
    return view;
}

void
ViewFactory::create_view(
    View &view,
    const std::string &key_def,
    const std::string &value_def,
    const std::string &order_def,
    unsigned int output_limit)
{
    for (auto def : split_args(key_def)) {
        string_trim(def);
        auto field = create_key_field(def);
        if (field->data_type() == DataType::VarString) {
            view.m_is_fixed_size = false;
        }
        field->set_offset(view.m_key_size);
        view.m_key_size += field->size();
        view.m_key_count++;
        view.m_fields.push_back(std::move(field));
    }

    for (auto def : split_args(value_def)) {
        string_trim(def);

        auto field = create_value_field(def);
        field->set_offset(view.m_key_size + view.m_value_size);
        view.m_value_size += field->size();
        view.m_value_count++;

        if (field->is_of_type<InOutValueField>()) {
            view.m_has_inout_fields = true;
        }

        view.m_fields.push_back(std::move(field));
    }

    if (view.m_has_inout_fields) {
        for (size_t i = 0; i < view.m_key_count; i++) {
            auto &field = view.m_fields[i];
            if (auto new_field = InOutKeyField::create_from(*field.get())) {
                new_field->set_name(field->name());
                new_field->set_offset(field->offset());
                field = std::move(new_field);
            }
        }
    }

    if (!order_def.empty()) {
        for (auto def : split_args(order_def)) {
            string_trim(def);

            const auto &pieces = string_split(def, "/", 2);

            const auto *field = view.find_field(pieces[0]);
            if (!field) {
                throw std::invalid_argument("cannot find compare field \"" + def + "\"");
            }

            View::OrderDirection dir;
            if (pieces.size() == 2) {
                if (pieces[1] == "asc") {
                    dir = View::OrderDirection::Ascending;
                } else if (pieces[1] == "desc") {
                    dir = View::OrderDirection::Descending;
                } else {
                    throw std::invalid_argument("invalid compare field dir \"" + def + "\"");
                }
            } else {
                // Default
                dir = View::OrderDirection::Ascending;
            }

            view.m_order_fields.push_back({field, dir});
        }
    }

    view.m_output_limit = output_limit;
}

} // namespace aggregator
} // namespace fdsdump
