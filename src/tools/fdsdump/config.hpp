#pragma once

#include <string>

struct aggregate_config_s
{
    bool key_src_ip;
    bool key_dst_ip;
    bool key_src_port;
    bool key_dst_port;
    bool key_protocol;
};

struct config_s
{
    std::string input_file;
    std::string input_filter;
    std::string output_filter;
    uint64_t max_input_records;
    aggregate_config_s aggregate_config;
    std::string sort_field;
    uint64_t max_output_records;
};

int
config_from_args(int argc, char **argv, config_s &config);
