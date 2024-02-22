/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Command line options
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <iostream>

#include <getopt.h>
#include <unistd.h>

#include <common/common.hpp>
#include <common/argParser.hpp>
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
    m_aggregation_values = "packets,bytes,flows";

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
	ArgParser parser;
	parser.add('r', "input", true);
	parser.add('F', "filter", true);
	parser.add('o', "output", true);
	parser.add('O', "order", true);
	parser.add('c', "limit", true);
	parser.add('A', "aggregation-keys", true);
	parser.add('S', "aggregation-values", true);
	parser.add("no-biflow-autoignore", false);
	parser.add('I', "stats-mode", false);

    Args args;
	try {
		args = parser.parse(argc, argv);
	} catch (const ArgParser::MissingArgument& missing) {
		throw OptionsException("Missing argument for " + missing.arg);
	} catch (const ArgParser::UnknownArgument& unknown) {
		throw OptionsException("Unknown argument " + unknown.arg);
	}

	if (args.has('r')) {
		for (const auto &arg : args.get_all('r')) {
			m_input_files.add_files(arg);
		}
	}

	if (args.has('c')) {
		auto maybe_value = parse_number<unsigned int>(args.get('c'));
		if (!maybe_value) {
			throw OptionsException("invalid -c/--limit value - not a number");
		}
		m_output_limit = *maybe_value;
	}

	if (args.has('o')) {
		m_output_specifier = args.get('o');
	}

	if (args.has('O')) {
		m_order_by = args.get('O');
	}

	if (args.has('F')) {
		m_input_filter = args.get('F');
	}

	if (args.has('A')) {
		m_aggregation_keys = args.get('A');
	}

	if (args.has('S')) {
		m_aggregation_values = args.get('S');
	}

	if (args.has("no-biflow-autoignore")) {
		m_biflow_autoignore = false;
	}

	if (args.has('I')) {
		m_mode = Mode::stats;
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
