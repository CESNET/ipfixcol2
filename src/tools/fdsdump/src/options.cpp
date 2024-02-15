
#include <getopt.h>
#include <unistd.h>

#include <options.hpp>

namespace fdsdump {

Options::Options()
{
    reset();
}

Options::Options(int argc, char *argv[])
{
    reset();
    parse(argc, argv);
    validate();
}

void Options::reset()
{
    m_mode = Mode::undefined;

    m_input_files.clear();
    m_input_filter.clear();

    m_output_limit = 0;
    m_output_specifier.clear();

    m_aggregation_keys.clear();
    m_aggregation_fields = "packets,bytes,flows";

    m_biflow_autoignore = true;

    m_order_by.clear();
}

/**
 * @brief Parse command line arguments.
 *
 * Previously specified values are not reset. Previous values might be
 * redefined or extended (e.g. files to process).
 * @param[in] argc Number of arguments
 * @param[in] argv Array of arguments
 */
void Options::parse(int argc, char *argv[])
{
    enum long_opts_vals {
        OPT_BIFLOW_AUTOIGNORE_OFF = 256, // Value that cannot colide with chars
    };
    const struct option long_opts[] = {
        {"filter",               required_argument, NULL, 'F'},
        {"output",               required_argument, NULL, 'o'},
        {"order",                required_argument, NULL, 'O'},
        {"limit",                required_argument, NULL, 'c'},
        {"no-biflow-autoignore", no_argument,       NULL, OPT_BIFLOW_AUTOIGNORE_OFF},
        {0, 0, 0, 0},
    };
    const char *short_opts = "r:c:o:O:F:A:S:I";
    int opt;

    while ((opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch (opt) {
        case 'r':
            m_input_files.add_files(optarg);
            break;
        case 'c':
            m_output_limit = std::stoull(optarg);
            break;
        case 'o':
            m_output_specifier = optarg;
            break;
        case 'O':
            m_order_by = optarg;
            break;
        case 'F':
            m_input_filter = optarg;
            break;
        case 'A':
            m_aggregation_keys = optarg;
            break;
        case 'I':
            m_mode = Mode::stats;
            break;
        case 'S':
            m_aggregation_fields = optarg;
            break;
        case OPT_BIFLOW_AUTOIGNORE_OFF:
            m_biflow_autoignore = false;
            break;
        case '?':
            throw OptionsException("invalid command line option(s)");
        default:
            throw OptionsException("getopts_long() returned unexpected value " + std::to_string(opt));
        }
    }

    if (optind < argc) {
        const char *arg = argv[optind];
        throw OptionsException("unknown argument '" + std::string(arg) + "'");
    }
}

void Options::validate()
{
    if (m_mode == Mode::stats) {
        // File statistics
        if (m_output_specifier.empty()) {
            m_output_specifier = "table";
        }

    } else if (!m_aggregation_keys.empty()) {
        // Record aggregation
        m_mode = Mode::aggregate;

        if (m_output_specifier.empty()) {
            m_output_specifier = "json";
        }

    } else {
        // Record listing
        m_mode = Mode::list;

        if (m_output_specifier.empty()) {
            m_output_specifier = "JSON-RAW";
        }
    }
}

} // fdsdump
