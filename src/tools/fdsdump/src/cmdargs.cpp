#include "cmdargs.hpp"
#include <cstdio>
#include <unistd.h>
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
        "  -a keys    Aggregator keys (e.g. srcip,dstip,srcport,dstport)\n"
        "  -s values  Aggregator values\n"
        "  -O fields  Field to sort on\n"
        "  -n num     Maximum number of records to write\n"
        "  -t num     Number of threads\n"
        "  -d         Translate IP addresses to domain names\n"
        ;

    printf("%s", usage);
}

CmdArgs
parse_cmd_args(int argc, char **argv)
{
    CmdArgs args = {};

    int opt;

    while ((opt = getopt(argc, argv, "hr:f:F:a:s:O:n:t:d")) != -1) {
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
            args.output_limit = std::stoi(optarg);
            break;
        case 't':
            args.num_threads = std::stoi(optarg);
            break;
        case 'd':
            args.translate_ip_addrs = true;
            break;
        default:
            throw ArgError("invalid option -" + std::to_string((char) opt));
        }
    }

    return args;
}
