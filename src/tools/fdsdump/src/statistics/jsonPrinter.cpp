/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief JSON statistics printer
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <iostream>
#include <stdexcept>
#include <string>

#include <statistics/jsonPrinter.hpp>

namespace fdsdump {
namespace statistics {

JsonPrinter::JsonPrinter(const std::string &args)
{
    if (!args.empty()) {
        throw std::invalid_argument("JSON output: options are not supported");
    }
}

JsonPrinter::~JsonPrinter()
{
}

void
JsonPrinter::print_prologue()
{
}

void
JsonPrinter::print_stats(const fds_file_stats &stats)
{
    const uint64_t flows_total = stats.recs_total - stats.recs_opts_total;

    std::cout << "{";

    std::cout << "\"recs_total\":" << stats.recs_total << ",";
    std::cout << "\"recs_flow_total\":" << flows_total << ",";
    std::cout << "\"recs_bf_total\":" << stats.recs_bf_total << ",";
    std::cout << "\"recs_opts_total\":" << stats.recs_opts_total << ",";
    std::cout << "\"recs_tcp\":" << stats.recs_tcp << ",";
    std::cout << "\"recs_udp\":" << stats.recs_udp << ",";
    std::cout << "\"recs_icmp\":" << stats.recs_icmp << ",";
    std::cout << "\"recs_other\":" << stats.recs_other << ",";
    std::cout << "\"recs_bf_tcp\":" << stats.recs_bf_tcp << ",";
    std::cout << "\"recs_bf_udp\":" << stats.recs_bf_udp << ",";
    std::cout << "\"recs_bf_icmp\":" << stats.recs_bf_icmp << ",";
    std::cout << "\"recs_bf_other\":" << stats.recs_bf_other << ",";
    std::cout << "\"pkts_total\":" << stats.pkts_total << ",";
    std::cout << "\"pkts_tcp\":" << stats.pkts_tcp << ",";
    std::cout << "\"pkts_udp\":" << stats.pkts_udp << ",";
    std::cout << "\"pkts_icmp\":" << stats.pkts_icmp << ",";
    std::cout << "\"pkts_other\":" << stats.pkts_other << ",";
    std::cout << "\"bytes_total\":" << stats.bytes_total << ",";
    std::cout << "\"bytes_tcp\":" << stats.bytes_tcp << ",";
    std::cout << "\"bytes_udp\":" << stats.bytes_udp << ",";
    std::cout << "\"bytes_icmp\":" << stats.bytes_icmp << ",";
    std::cout << "\"bytes_other\":" << stats.bytes_other;

    std::cout << "}\n";
}

void
JsonPrinter::print_epilogue()
{
}

} // statistics
} // fdsdump
