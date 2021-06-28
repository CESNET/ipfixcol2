#include "config.hpp"
#include <cstdio>
#include <vector>

class ArgParser
{
public:
    ArgParser(int argc, char **argv);

    bool
    next();

    std::string
    arg();

private:
    int m_argc;
    char **m_argv;
    int m_arg_index = 0;
};

ArgParser::ArgParser(int argc, char **argv)
    : m_argc(argc), m_argv(argv)
{
}

bool
ArgParser::next()
{
    if (m_arg_index == m_argc - 1) {
        return false;
    }
    m_arg_index++;
    return true;
}

std::string
ArgParser::arg()
{
    return m_argv[m_arg_index];
}

static void
usage()
{
    const char *text =
    "Usage: fdsdump [options]\n"
    "  -h         Show this help\n"
    "  -r path    FDS input file\n"
    "  -f expr    Input filter\n"
    "  -of expr   Output filter\n"
    "  -c num     Max number of records to read\n"
    "  -a keys    Aggregator keys (e.g. srcip,dstip,srcport,dstport)\n"
    ;
    fprintf(stderr, text);
}

static void
missing_arg(const char *opt)
{
    fprintf(stderr, "Missing argument for %s", opt);
}

static std::vector<std::string>
string_split(const std::string &str, const std::string &delimiter)
{
    std::vector<std::string> pieces;
    std::size_t pos = 0;
    for (;;) {
        std::size_t next_pos = str.find(delimiter, pos);
        if (next_pos == std::string::npos) {
            pieces.emplace_back(str.begin() + pos, str.end());
            break;
        }
        pieces.emplace_back(str.begin() + pos, str.begin() + next_pos);
        pos = next_pos + 1;
    }
    return pieces;
}

static void
parse_aggregate_config(const std::string &options, aggregate_config_s &aggregate_config)
{
    aggregate_config = {};

    for (const auto &key : string_split(options, ",")) {
        if (key == "srcip") {
            aggregate_config.key_src_ip = true;
        } else if (key == "dstip") {
            aggregate_config.key_dst_ip = true;
        } else if (key == "srcport") {
            aggregate_config.key_src_port = true;
        } else if (key == "dstport") {
            aggregate_config.key_dst_port = true;
        } else if (key == "proto") {
            aggregate_config.key_protocol = true;
        }
    }
}

int
config_from_args(int argc, char **argv, config_s &config)
{
    config = {};
    config.aggregate_config.key_src_ip = true;
    config.aggregate_config.key_dst_ip = true;
    config.aggregate_config.key_src_port = true;
    config.aggregate_config.key_dst_port = true;
    config.aggregate_config.key_protocol = true;

    ArgParser parser{argc, argv};
    while (parser.next()) {
        if (parser.arg() == "-h") {
            usage();
            return 1;
        } else if (parser.arg() == "-r") {
            if (!parser.next()) {
                missing_arg("-r");
                return 1;
            }
            config.input_file = parser.arg();
        } else if (parser.arg() == "-f") {
            if (!parser.next()) {
                missing_arg("-f");
                return 1;
            }
            config.input_filter = parser.arg();
        } else if (parser.arg() == "-of") {
            if (!parser.next()) {
                missing_arg("-of");
                return 1;
            }
            config.output_filter = parser.arg();
        } else if (parser.arg() == "-c") {
            if (!parser.next()) {
                missing_arg("-c");
                return 1;
            }
            config.max_input_records = std::stoul(parser.arg());
        } else if (parser.arg() == "-a") {
            if (!parser.next()) {
                missing_arg("-a");
                return 1;
            }
            parse_aggregate_config(parser.arg(), config.aggregate_config);
        } else {
            fprintf(stderr, "Unknown argument %s\n", parser.arg().c_str());
            return 1;
        }
    }

    if (config.input_file.empty()) {
        usage();
        return 1;
    }

    if (config.input_filter.empty()) {
        config.input_filter = "true";
    }

    if (config.output_filter.empty()) {
        config.output_filter = "true";
    }

    return 0;
}