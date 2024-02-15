/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Select allocator implementation
 */

#pragma once

#ifndef FDSDUMP_USE_STD_ALLOCATOR

#include <aggregator/arenaAllocator.hpp>

namespace fdsdump {
namespace aggregator {

using Allocator = ArenaAllocator;

} // aggregator
} // fdsdump

#else

#include <aggregator/stdAllocator.hpp>

namespace fdsdump {
namespace aggregator {

using Allocator = StdAllocator;

} // aggregator
} // fdsdump

#endif
