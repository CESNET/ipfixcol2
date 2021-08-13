#include "view/tableprinter.hpp"

#include <cstdio>

#include <arpa/inet.h>
#include <netdb.h>

#include "ipfix/informationelements.hpp"

static int
get_width(const ViewField &field)
{
    //TODO: These are just randomly choosen numbers so the output looks somewhat nice for now
    switch (field.data_type) {
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
    case DataType::DateTime:
        return 30;
    default:
        assert(0);
    }
}

static void
datetime_to_str(char *buffer, uint64_t ts_millisecs)
{
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
    std::size_t n = strftime(buffer, 64, "%Y-%m-%d %H:%M:%S", &tm);
    assert(n > 0);
    sprintf(&buffer[n], ".%03lu", msecs_part);
}

static bool
translate_ipv4_address(uint8_t *address, char *buffer)
{
    //TODO: This should probably be done in parallel by a thread pool and we should probably cache the results

    sockaddr_in sa = {};
    sa.sin_family = AF_INET;
    memcpy((void *) &sa.sin_addr, address, 4);

    //TODO: Have a look at the hard coded buffer size
    int ret = getnameinfo((sockaddr *) &sa, sizeof(sa), buffer, 64, NULL, 0, NI_NAMEREQD);

    if (ret == 0) {
        return true;
    } else {
        return false;
    }
}

static bool
translate_ipv6_address(uint8_t *address, char *buffer)
{
    //TODO: This should probably be done in parallel by a thread pool and we should probably cache the results

    sockaddr_in6 sa = {};
    sa.sin6_family = AF_INET6;
    memcpy((void *) &sa.sin6_addr, address, 16);

    //TODO: Have a look at the hard coded buffer size
    int ret = getnameinfo((sockaddr *) &sa, sizeof(sa), buffer, 64, NULL, 0, NI_NAMEREQD);

    if (ret == 0) {
        return true;
    } else {
        return false;
    }
}

static void
print_value(const ViewField &field, ViewValue &value, char *buffer, bool translate_ip_addrs)
{
    // Print protocol name
    if (field.kind == ViewFieldKind::VerbatimKey && field.pen == IPFIX::iana && field.id == IPFIX::protocolIdentifier) {
        protoent *proto = getprotobynumber(value.u8);
        if (proto) {
            sprintf(buffer, "%s", proto->p_name);
            return;
        }

    } else if (field.kind == ViewFieldKind::BiflowDirectionKey) {
        switch (value.u8) {
        case 0: sprintf(buffer, ""); break;
        case 1: sprintf(buffer, "->"); break;
        case 2: sprintf(buffer, "<-"); break;
        }
        return;
    }

    //TODO: Have a look at the hard coded sizes everywhere...
    switch (field.data_type) {
    case DataType::Unsigned8:
        sprintf(buffer, "%hhu", value.u8);
        break;
    case DataType::Unsigned16:
        sprintf(buffer, "%hu", value.u16);
        break;
    case DataType::Unsigned32:
        sprintf(buffer, "%u", value.u32);
        break;
    case DataType::Unsigned64:
        sprintf(buffer, "%lu", value.u64);
        break;
    case DataType::Signed8:
        sprintf(buffer, "%hhd", value.i8);
        break;
    case DataType::Signed16:
        sprintf(buffer, "%hd", value.i16);
        break;
    case DataType::Signed32:
        sprintf(buffer, "%d", value.i32);
        break;
    case DataType::Signed64:
        sprintf(buffer, "%ld", value.i64);
        break;
    case DataType::IPAddress:
        if (value.ip.length == 4) {
            if (!translate_ip_addrs || !translate_ipv4_address(value.ip.address, buffer)) {
                inet_ntop(AF_INET, value.ip.address, buffer, 64);
            }
        } else if (value.ip.length == 16) {
            if (!translate_ip_addrs || !translate_ipv6_address(value.ip.address, buffer)) {
                inet_ntop(AF_INET6, value.ip.address, buffer, 64);
            }
        }
        break;
    case DataType::IPv4Address:
        if (!translate_ip_addrs || !translate_ipv4_address(value.ipv4, buffer)) {
            inet_ntop(AF_INET, value.ipv4, buffer, 64);
        }
        break;
    case DataType::IPv6Address:
        if (!translate_ip_addrs || !translate_ipv6_address(value.ipv6, buffer)) {
            inet_ntop(AF_INET6, value.ipv6, buffer, 64);
        }
        break;
    case DataType::String128B:
        sprintf(buffer, "%.128s", value.str);
        break;
    case DataType::DateTime:
        datetime_to_str(buffer, value.ts_millisecs);
        break;

    default: assert(0);
    }
}

TablePrinter::TablePrinter(ViewDefinition view_def)
    : m_view_def(view_def)
{
}

TablePrinter::~TablePrinter()
{
}

void
TablePrinter::print_prologue()
{
    for (const auto &field : m_view_def.key_fields) {
        printf("%*s ", get_width(field), field.name.c_str());
    }

    for (const auto &field : m_view_def.value_fields) {
        printf("%*s ", get_width(field), field.name.c_str());
    }

    printf("\n");
}

void
TablePrinter::print_record(uint8_t *record)
{
    char buffer[1024] = {0}; //TODO: We might want to do this some other way
    ViewValue *value = (ViewValue *) record;

    for (const auto &field : m_view_def.key_fields) {
        print_value(field, *value, buffer, m_translate_ip_addrs);
        advance_value_ptr(value, field.size);
        printf("%*s ", get_width(field), buffer);
    }

    for (const auto &field : m_view_def.value_fields) {
        print_value(field, *value, buffer, m_translate_ip_addrs);
        advance_value_ptr(value, field.size);
        printf("%*s ", get_width(field), buffer);
    }

    printf("\n");
}

void
TablePrinter::print_epilogue()
{
}
