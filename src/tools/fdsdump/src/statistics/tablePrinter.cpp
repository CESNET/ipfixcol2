/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Table statistics printer
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>

#include <statistics/tablePrinter.hpp>

namespace fdsdump {
namespace statistics {

TablePrinter::TablePrinter(const std::string &args)
{
    if (!args.empty()) {
        throw std::invalid_argument("JSON output: options are not supported");
    }
}

TablePrinter::~TablePrinter()
{
}

void
TablePrinter::print_prologue()
{
}

void
TablePrinter::print_stats(const fds_file_stats &stats)
{
    struct TableEntry {
        std::string name;
        uint64_t value;
        uint64_t base;
        bool show_base;

        // Entry without percentage of a base
        TableEntry(const std::string &_name, uint64_t _value)
            : name(_name), value(_value), base(0), show_base(false)
            {};

        // Entry with percentage of a base
        TableEntry(const std::string _name, uint64_t _value, uint64_t _base)
            : name(_name), value(_value), base(_base), show_base(true)
            {};

        // Empty line (seperator)
        TableEntry()
            : name(), value(0), base(0), show_base(false)
            {};
    };

    std::vector<TableEntry> entries;

    const uint64_t flow_recs = stats.recs_total - stats.recs_opts_total;

    entries.emplace_back("All records",           stats.recs_total);
    entries.emplace_back("Flow records",          flow_recs,             stats.recs_total);
    entries.emplace_back("Biflow-only records",   stats.recs_bf_total,   stats.recs_total);
    entries.emplace_back("Options records",       stats.recs_opts_total, stats.recs_total);
    entries.emplace_back();
    entries.emplace_back("All TCP records",       stats.recs_tcp,   flow_recs);
    entries.emplace_back("All UDP records",       stats.recs_udp,   flow_recs);
    entries.emplace_back("All ICMP records",      stats.recs_icmp,  flow_recs);
    entries.emplace_back("All other records",     stats.recs_other, flow_recs);
    entries.emplace_back();
    entries.emplace_back("Uniflow TCP records",   stats.recs_tcp   - stats.recs_bf_tcp,   flow_recs);
    entries.emplace_back("Uniflow UDP records",   stats.recs_udp   - stats.recs_bf_udp,   flow_recs);
    entries.emplace_back("Uniflow ICMP records",  stats.recs_icmp  - stats.recs_bf_icmp,  flow_recs);
    entries.emplace_back("Uniflow other records", stats.recs_other - stats.recs_bf_other, flow_recs);
    entries.emplace_back("Biflow TCP records",    stats.recs_bf_tcp,   flow_recs);
    entries.emplace_back("Biflow UDP records",    stats.recs_bf_udp,   flow_recs);
    entries.emplace_back("Biflow ICMP records",   stats.recs_bf_icmp,  flow_recs);
    entries.emplace_back("Biflow other records",  stats.recs_bf_other, flow_recs);
    entries.emplace_back();
    entries.emplace_back("All packets",           stats.pkts_total);
    entries.emplace_back("TCP packets",           stats.pkts_tcp,    stats.pkts_total);
    entries.emplace_back("UDP packets",           stats.pkts_udp,    stats.pkts_total);
    entries.emplace_back("ICMP packets",          stats.pkts_icmp,   stats.pkts_total);
    entries.emplace_back("Other packets",         stats.pkts_other,  stats.pkts_total);
    entries.emplace_back();
    entries.emplace_back("All bytes",             stats.bytes_total);
    entries.emplace_back("TCP bytes",             stats.bytes_tcp,   stats.bytes_total);
    entries.emplace_back("UDP bytes",             stats.bytes_udp,   stats.bytes_total);
    entries.emplace_back("ICMP bytes",            stats.bytes_icmp,  stats.bytes_total);
    entries.emplace_back("Other bytes",           stats.bytes_other, stats.bytes_total);

    const auto elem_longest_name = std::max_element(entries.begin(), entries.end(),
        [](const TableEntry &lhs, const TableEntry &rhs) {
            return lhs.name.length() < rhs.name.length();
        });
    const auto elem_longest_value = std::max_element(entries.begin(), entries.end(),
        [](const TableEntry &lhs, const TableEntry &rhs) {
            return std::to_string(lhs.value).length() < std::to_string(rhs.value).length();
        });
    const size_t max_name_len = elem_longest_name->name.length();
    const size_t max_value_len = std::to_string(elem_longest_value->value).length();

    for (const auto &entry : entries) {
        if (entry.name.length() == 0) {
            std::cout << std::endl;
            continue;
        }

        std::cout << std::setw(max_name_len) << std::right << entry.name << ":  ";
        std::cout << std::setw(max_value_len) << entry.value;

        if (!entry.show_base) {
            std::cout << std::endl;
            continue;
        }

        std::cout << std::fixed << std::setprecision(2) << " (" << std::setw(6);
        std::cout << double(entry.value) / double(entry.base) * 100;
        std::cout << "%)" << std::endl;
    }
}

void
TablePrinter::print_epilogue()
{
}

} // statistics
} // fdsdump
