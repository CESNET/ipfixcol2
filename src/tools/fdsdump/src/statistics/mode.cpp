/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Statistics mode
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <iostream>

#include <common/common.hpp>
#include <common/flowProvider.hpp>

#include <statistics/mode.hpp>
#include <statistics/printer.hpp>

namespace fdsdump {
namespace statistics {

static void stats_merge(
    fds_file_stats &dst,
    const fds_file_stats &src)
{
    dst.recs_total += src.recs_total;
    dst.recs_bf_total += src.recs_bf_total;
    dst.recs_opts_total += src.recs_opts_total;
    dst.bytes_total += src.bytes_total;
    dst.pkts_total += src.pkts_total;
    dst.recs_tcp += src.recs_tcp;
    dst.recs_udp += src.recs_udp;
    dst.recs_icmp += src.recs_icmp;
    dst.recs_other += src.recs_other;
    dst.recs_bf_tcp += src.recs_bf_tcp;
    dst.recs_bf_udp += src.recs_bf_udp;
    dst.recs_bf_icmp += src.recs_bf_icmp;
    dst.recs_bf_other += src.recs_bf_other;
    dst.bytes_tcp += src.bytes_tcp;
    dst.bytes_udp += src.bytes_udp;
    dst.bytes_icmp += src.bytes_icmp;
    dst.bytes_other += src.bytes_other;
    dst.pkts_tcp += src.pkts_tcp;
    dst.pkts_udp += src.pkts_udp;
    dst.pkts_icmp += src.pkts_icmp;
    dst.pkts_other += src.pkts_other;
}

void
mode_statistics(const Options &opts)
{
    auto printer = printer_factory(opts.get_output_specifier());
    std::vector<std::string> file_names = glob_files(opts.get_input_file_patterns());
    unique_file file {nullptr, &fds_file_close};
    fds_file_stats stats {};

    file.reset(fds_file_init());
    if (!file) {
        throw std::runtime_error("fds_file_init() has failed");
    }

    for (const auto &file_name : file_names) {
        const uint32_t flags = FDS_FILE_READ | FDS_FILE_NOASYNC;
        const fds_file_stats *file_stats;
        int ret;

        ret = fds_file_open(file.get(), file_name.c_str(), flags);
        if (ret != FDS_OK) {
            const std::string err_msg = fds_file_error(file.get());
            std::cerr << "fds_file_open('" << file_name << "') failed: " << err_msg << std::endl;
            continue;
        }

        file_stats = fds_file_stats_get(file.get());
        if (!file_stats) {
            std::cerr << "fds_file_stats_get('" << file_name << "') failed" << std::endl;
            continue;
        }

        stats_merge(stats, *file_stats);
    }

    printer->print_prologue();
    printer->print_stats(stats);
    printer->print_epilogue();

    return;
}

} // statistics
} // fdsdump
