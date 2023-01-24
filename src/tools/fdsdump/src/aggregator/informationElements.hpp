/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Information element constants
 */
#pragma once

#include <cstdint>

namespace fdsdump {
namespace aggregator {

namespace IPFIX {

enum PEN : uint32_t
{
    iana = 0
};

enum ID : uint16_t
{
    octetDeltaCount = 1,
    packetDeltaCount = 2,
    deltaFlowCount = 3,
    protocolIdentifier = 4,
    sourceTransportPort = 7,
    sourceIPv4Address = 8,
    destinationTransportPort = 11,
    destinationIPv4Address = 12,
    sourceIPv6Address = 27,
    destinationIPv6Address = 28,
    flowStartSeconds = 150,
    flowEndSeconds = 151,
    flowStartMilliseconds = 152,
    flowEndMilliseconds = 153,
    flowStartMicroseconds = 154,
    flowEndMicroseconds = 155,
    flowStartNanoseconds = 156,
    flowEndNanoseconds = 157
};

}

} // aggregator
} // fdsdump