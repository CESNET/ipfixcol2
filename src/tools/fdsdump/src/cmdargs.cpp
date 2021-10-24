/**
 * \file src/cmdargs.cpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Command line arguments
 *
 * Copyright (C) 2021 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */
#include "cmdargs.hpp"
#include <cstdio>
#include <unistd.h>
#include <thread>
#include <algorithm>
#include "common.hpp"

void
print_usage()
{
    const char *usage =
        "Usage: fdsdump [options]\n"
        "  -h         Show this help\n"
        "  -r path    FDS input file pattern (glob)\n"
        "  -f expr    Input filter\n"
        "  -F expr    Output filter\n"
        "  -a keys    Aggregator keys (e.g. srcip,dstip,srcport,dstport)\n" //TODO: Better explain all the possible options and uses
        "  -s values  Aggregator values\n"
        "  -O fields  Field to sort on\n"
        "  -n num     Maximum number of records to write\n"
        "  -t num     Number of threads\n"
        "  -d         Translate IP addresses to domain names\n"
        "  -o mode    Output mode (table, json, csv)\n"
        "  -I         Collect and print basic stats\n"
        ;

    printf("%s", usage);
}

static CmdArgs
default_args()
{
    CmdArgs args = {};

    args.aggregate_keys = "srcip,srcport,dstip,dstport,proto";
    args.aggregate_values = "packets,bytes";
    args.input_filter = "true";
    args.output_filter = "true";
    args.num_threads = std::max<unsigned int>(std::thread::hardware_concurrency(), 1); //TODO
    args.output_mode = "table";

    return args;
}

CmdArgs
parse_cmd_args(int argc, char **argv)
{
    CmdArgs args = default_args();

    int opt;

    while ((opt = getopt(argc, argv, "hr:f:F:a:s:O:n:t:do:I")) != -1) {
        switch (opt) {
        case 'h':
            args.print_help = true;
            break;
        case 'r':
            args.input_file_patterns.push_back(optarg);
            break;
        case 'f':
            args.input_filter = optarg;
            break;
        case 'F':
            args.output_filter = optarg;
            break;
        case 'a':
            args.aggregate_keys = optarg;
            break;
        case 's':
            args.aggregate_values = optarg;
            break;
        case 'O':
            args.sort_fields = optarg;
            break;
        case 'n':
            args.output_limit = std::stoi(optarg); //TODO: Probably turn the possible exception into an ArgError
            break;
        case 't':
            args.num_threads = std::stoi(optarg); //TODO: Probably turn the possible exception into an ArgError
            break;
        case 'd':
            args.translate_ip_addrs = true;
            break;
        case 'o':
            args.output_mode = optarg;
            break;
        case 'I':
            args.stats = true;
            break;
        default:
            throw ArgError("invalid option -" + std::string(1, opt));
        }
    }

    return args;
}
