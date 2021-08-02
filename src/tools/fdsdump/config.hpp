#pragma once

#include <string>
#include <vector>
#include <libfds.h>
#include "view.hpp"

struct Config
{
    std::string input_file;
    std::string input_filter;
    std::string output_filter;
    uint64_t max_input_records;
    ViewDefinition view_def;
    std::string sort_field;
    uint64_t max_output_records;
    bool translate_ip_addrs;
};

int
config_from_args(int argc, char **argv, Config &config, fds_iemgr_t &iemgr);
