#pragma once

#include <vector>
#include <string>

struct CmdArgs {
    std::vector<std::string> input_file_patterns;
    std::string input_filter;
    std::string aggregate_keys;
    std::string aggregate_values;
    std::string output_filter;
    std::string sort_fields;
    unsigned int num_threads;
    unsigned int output_limit;
    bool translate_ip_addrs;
    bool print_help;
};

void
print_usage();

CmdArgs
parse_cmd_args(int argc, char **argv);
