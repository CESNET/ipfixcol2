#pragma once

#include <cstdint>

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
    destinationIPv6Address = 28
};

};
