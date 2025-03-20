/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief View print functions
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <aggregator/print.hpp>
#include <aggregator/view.hpp>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

#include <cctype>
#include <iostream>

namespace fdsdump {
namespace aggregator {

int
get_width(const Field &field)
{
    //TODO: These are just randomly choosen numbers so the output looks somewhat nice for now
    switch (field.data_type()) {
    case DataType::Unsigned8:
    case DataType::Signed8:
        return 5;
    case DataType::Unsigned16:
    case DataType::Signed16:
        return 5;
    case DataType::Unsigned32:
    case DataType::Signed32:
        return 8;
    case DataType::Unsigned64:
    case DataType::Signed64:
        return 12;
    case DataType::IPAddress:
    case DataType::IPv6Address:
        return 39;
    case DataType::IPv4Address:
        return 15;
    case DataType::String128B:
        return 40;
    case DataType::Octets128B:
        return 40;
    case DataType::DateTime:
        return 30;
    case DataType::MacAddress:
        return 17;
    case DataType::VarString:
        return 40;
    default:
        assert(0);
    }
}

std::string
datetime_to_str(uint64_t ts_millisecs)
{
    char buffer[128];
    static_assert(sizeof(uint64_t) == sizeof(time_t), "Assumed that time_t is uint64_t, but it's not");

    uint64_t secs = ts_millisecs / 1000;
    uint64_t msecs_part = ts_millisecs % 1000;
    if (msecs_part < 10) {
        msecs_part *= 100;
    } else if (msecs_part < 100) {
        msecs_part *= 10;
    }

    tm tm;
    localtime_r(reinterpret_cast<time_t *>(&secs), &tm);
    //TODO: The format should probably be configrable
    //TODO: Have a look at the hard coded buffer size
    std::size_t n = strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    assert(n > 0);
    snprintf(&buffer[n], sizeof(buffer) - n, ".%03lu", msecs_part);

    return buffer;
}

std::string
char2hex(const char byte)
{
    const uint8_t nibble_high = (byte & 0xF0) >> 4;
    const uint8_t nibble_low = (byte & 0x0F);
    const char char_high = (nibble_high < 10) ? ('0' + nibble_high) : ('A' - 10 + nibble_high);
    const char char_low = (nibble_low < 10) ? ('0' + nibble_low) : ('A' - 10 + nibble_low);

    return std::string().append(1, char_high).append(1, char_low);
}

static std::string
ipv4_to_str(const uint8_t addr[4])
{
    char tmp[INET_ADDRSTRLEN];

    if (!inet_ntop(AF_INET, addr, tmp, sizeof(tmp))) {
        return "<invalid>";
    } else {
        return tmp;
    }
}

static std::string
ipv6_to_str(const uint8_t addr[16])
{
    char tmp[INET6_ADDRSTRLEN];

    if (!inet_ntop(AF_INET6, addr, tmp, sizeof(tmp))) {
        return "<invalid>";
    } else {
        return tmp;
    }
}

static std::string
mac_to_str(const uint8_t addr[6])
{
    char buffer[sizeof("XX:XX:XX:XX:XX:XX")];

    snprintf(
        buffer, sizeof(buffer),
        "%02x:%02x:%02x:%02x:%02x:%02x",
        addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    return buffer;
}

static std::string
octetarray_to_str(const char array[128])
{
    std::string result {"0x"};
    size_t last_byte;

    // Find first non-zero byte from the end (at least one byte must be printed)
    for (last_byte = 128; last_byte > 1; --last_byte) {
        if (array[last_byte - 1] != 0) {
            break;
        }
    }

    for (size_t i = 0; i < last_byte; ++i) {
        const char byte = array[i];
        result.append(char2hex(byte));
    }

    return result;
}

static std::string
string_to_str(const char array[128])
{
    std::string result;
    size_t last_byte;

    for (last_byte = 128; last_byte > 0; --last_byte) {
        if (array[last_byte - 1] != 0) {
            break;
        }
    }

    for (size_t i = 0; i < last_byte; ++i) {
        const char byte = array[i];

        if (std::isprint(byte)) {
            result.append(1, byte);
        } else {
            result.append("\\x");
            result.append(char2hex(byte));
        }
    }

    return result;
}

static std::string
varstring_to_str(const char *text, uint32_t len)
{
    std::string result;
    result.reserve(len);

    for (uint32_t i = 0; i < len; ++i) {
        const char byte = text[i];

        if (std::isprint(byte)) {
            result.append(1, byte);
        } else {
            result.append("\\x");
            result.append(char2hex(byte));
        }
    }

    return result;
}

void
print_value(const Field &field, Value &value, std::string &buffer)
{
    // Print protocol name
    // if (field.kind == ViewFieldKind::VerbatimKey && field.pen == IPFIX::iana && field.id == IPFIX::protocolIdentifier) {
    //     protoent *proto = getprotobynumber(value.u8);
    //     if (proto) {
    //         buffer.append(proto->p_name);
    //         return;
    //     }

    // } else if (field.kind == ViewFieldKind::BiflowDirectionKey) {
    //     switch (value.u8) {
    //     case 0: buffer.append(" "); break;
    //     case 1: buffer.append("->"); break;
    //     case 2: buffer.append("<-"); break;
    //     }
    //     return;
    // }

    switch (field.data_type()) {
    case DataType::Unsigned8:
        buffer.append(std::to_string(static_cast<unsigned int>(value.u8)));
        break;
    case DataType::Unsigned16:
        buffer.append(std::to_string(value.u16));
        break;
    case DataType::Unsigned32:
        buffer.append(std::to_string(value.u32));
        break;
    case DataType::Unsigned64:
        buffer.append(std::to_string(value.u64));
        break;
    case DataType::Signed8:
        buffer.append(std::to_string(static_cast<int>(value.i8)));
        break;
    case DataType::Signed16:
        buffer.append(std::to_string(value.i16));
        break;
    case DataType::Signed32:
        buffer.append(std::to_string(value.i32));
        break;
    case DataType::Signed64:
        buffer.append(std::to_string(value.i64));
        break;
    case DataType::IPAddress:
        buffer.append(value.ip.to_string());
        break;
    case DataType::IPv4Address:
        buffer.append(ipv4_to_str(value.ipv4));
        break;
    case DataType::IPv6Address:
        buffer.append(ipv6_to_str(value.ipv6));
        break;
    case DataType::MacAddress:
        buffer.append(mac_to_str(value.mac));
        break;
    case DataType::String128B:
        buffer.append(string_to_str(value.str));
        break;
    case DataType::Octets128B:
        buffer.append(octetarray_to_str(value.str));
        break;
    case DataType::DateTime:
        buffer.append(datetime_to_str(value.ts_millisecs));
        break;
    case DataType::VarString:
        buffer.append(varstring_to_str(value.varstr.text, value.varstr.len));
        break;
    default: assert(0);
    }
}

void
debug_print_record(const View &view, uint8_t *record)
{
    for (const auto &pair : view.iter_fields(record)) {
        std::string value;
        print_value(pair.field, pair.value, value);
        std::cerr << pair.field.name()
            << "[size=" << pair.field.size()
            << ", offset=" << pair.field.offset()
            << "] = "
            << value << "\n";
    }
}


} // aggregator
} // fdsdump
