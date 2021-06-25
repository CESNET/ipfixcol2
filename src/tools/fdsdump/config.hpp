#include <string>

struct config_s
{
    std::string input_file;
    std::string input_filter;
    std::string output_filter;
    uint64_t max_input_records;
};

int
config_from_args(int argc, char **argv, config_s &config);
