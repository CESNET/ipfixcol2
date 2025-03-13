/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Select hash table implementation
 */

#pragma once

#if defined(__SSE2__) && !defined(FDSDUMP_USE_STD_HASHTABLE)

#include <aggregator/fastHashTable.hpp>

namespace fdsdump {
namespace aggregator {

using HashTable = FastHashTable;

} // aggregator
} // fdsdump

#else

#include <aggregator/stdHashTable.hpp>

namespace fdsdump {
namespace aggregator {

using HashTable = StdHashTable;

} // aggregator
} // fdsdump

#endif
