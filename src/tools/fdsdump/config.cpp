#include "config.hpp"
#include <cstdio>

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
    "  -h        Show this help\n"
    "  -r path   FDS input file\n"
    "  -f expr   Input filter\n"
    "  -of expr  Output filter\n"
    "  -c num    Max number of records to read\n"
    ;
    fprintf(stderr, text);
}

static void
missing_arg(const char *opt)
{
    fprintf(stderr, "Missing argument for %s", opt);
}

int
config_from_args(int argc, char **argv, config_s &config)
{
    config = {};

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