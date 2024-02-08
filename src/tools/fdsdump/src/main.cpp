
#include <cstdlib>
#include <iostream>
#include <vector>
#include <unistd.h>

#include <libfds.h>

#include <common/common.hpp>
#include <common/filelist.hpp>
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
    } catch (const std::exception &ex) {
        std::cerr << "ERROR: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}