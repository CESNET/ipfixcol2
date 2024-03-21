
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include "ipaddr.hpp"

namespace fdsdump {

IPAddr::IPAddr(const std::string &str)
{
    struct in_addr ip4;
    struct in6_addr ip6;

    if (inet_pton(AF_INET, str.c_str(), &ip4) == 1) {
        u64[0] = 0;
        u32[2] = htonl(0x0000FFFF);
        memcpy(&u32[3], &ip4, 4U);
        return;
    }

    if (inet_pton(AF_INET6, str.c_str(), &ip6) == 1) {
        memcpy(&u8[0], &ip6, 16U);
        return;
    }

    throw std::invalid_argument("IPAddr: Not an IP address!");
}

std::string
IPAddr::to_string() const
{
    char buffer[INET6_ADDRSTRLEN];
    if (is_ip4()) {
        inet_ntop(AF_INET, &u32[3], buffer, sizeof(buffer));
    } else {
        inet_ntop(AF_INET6, &u8[0], buffer, sizeof(buffer));
    }
    return std::string(buffer);
}

} // fdsdump
