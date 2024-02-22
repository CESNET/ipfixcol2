/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief FDSdump main entrypoint
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdlib>
#include <iostream>
#include <unistd.h>

#include <libfds.h>

#include <common/common.hpp>
#include <common/logger.hpp>
#include <lister/lister.hpp>
#include <aggregator/mode.hpp>
#include <statistics/mode.hpp>

#include <options.hpp>

using namespace fdsdump;

int
main(int argc, char *argv[])
{
    try {
        Options options {argc, argv};

        Logger::get_instance().set_log_level(options.get_log_level());

        if (options.get_help_flag()) {
            Options::print_usage();
            return EXIT_SUCCESS;
        }

        switch (options.get_mode()) {
        case Options::Mode::list:
            lister::mode_list(options);
            break;
        case Options::Mode::aggregate:
            aggregator::mode_aggregate(options);
            break;
        case Options::Mode::stats:
            statistics::mode_statistics(options);
            break;
        default:
            throw std::runtime_error("Invalid mode");
        }

    } catch (const OptionsException &ex) {
        std::cerr << "ERROR: " << ex.what() << std::endl;
        Options::print_usage();
        return EXIT_FAILURE;

    } catch (const std::exception &ex) {
        std::cerr << "ERROR: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
