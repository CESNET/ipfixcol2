#define XXH_INLINE_ALL

#include "aggregator.hpp"
#include "information_elements.hpp"
#include "xxhash.h"
#include <algorithm>
#include <cstring>
#include <iostream>

static IPAddress
make_ipv4_address(uint8_t *address)
{
    IPAddress a = {};
    a.length = 4;
    std::memcpy(a.address, address, 4);
    return a;
}

static IPAddress
make_ipv6_address(uint8_t *address)
{
    IPAddress a = {};
    a.length = 16;
    std::memcpy(a.address, address, 16);
    return a;
}

static void
memcpy_bits(uint8_t *dst, uint8_t *src, unsigned n_bits)
{
    unsigned n_bytes = (n_bits + 7) >> 3;
    unsigned rem_bits = 8 - (n_bits & 0x07);
    memcpy(dst, src, n_bytes);
    if (rem_bits != 8) {
        dst[n_bytes - 1] &= (0xFF >> rem_bits) << rem_bits;
    }
}

static uint64_t
get_uint(fds_drec_field &field)
{
    uint64_t tmp;
    int rc = fds_get_uint_be(field.data, field.size, &tmp);
    assert(rc == FDS_OK);
    return tmp;
}

static int64_t
get_int(fds_drec_field &field)
{
    int64_t tmp;
    int rc = fds_get_int_be(field.data, field.size, &tmp);
    assert(rc == FDS_OK);
    return tmp;
}

static int
build_key(const ViewDefinition &view_def, fds_drec &drec, uint8_t *key_buffer, Direction direction)
{
    ViewValue *key_value = reinterpret_cast<ViewValue *>(key_buffer);
    fds_drec_field drec_field;

    for (const auto &view_field : view_def.key_fields) {

        switch (view_field.kind) {
        case ViewFieldKind::VerbatimKey:
            if (fds_drec_find(&drec, view_field.pen, view_field.id, &drec_field) == FDS_EOC) {
                return 0;
            }

            switch (view_field.data_type) {
            case DataType::Unsigned8:
                key_value->u8 = get_uint(drec_field);
                advance_value_ptr(key_value, sizeof(key_value->u8));
                break;
            case DataType::Unsigned16:
                key_value->u16 = get_uint(drec_field);
                advance_value_ptr(key_value, sizeof(key_value->u16));
                break;
            case DataType::Unsigned32:
                key_value->u32 = get_uint(drec_field);
                advance_value_ptr(key_value, sizeof(key_value->u32));
                break;
            case DataType::Unsigned64:
                key_value->u64 = get_uint(drec_field);
                advance_value_ptr(key_value, sizeof(key_value->u64));
                break;
            case DataType::Signed8:
                key_value->i8 = get_int(drec_field);
                advance_value_ptr(key_value, sizeof(key_value->i8));
                break;
            case DataType::Signed16:
                key_value->i16 = get_int(drec_field);
                advance_value_ptr(key_value, sizeof(key_value->i16));
                break;
            case DataType::Signed32:
                key_value->i32 = get_int(drec_field);
                advance_value_ptr(key_value, sizeof(key_value->i32));
                break;
            case DataType::Signed64:
                key_value->i64 = get_int(drec_field);
                advance_value_ptr(key_value, sizeof(key_value->i64));
                break;
            case DataType::String128B:
                memset(key_value->str, 0, 128);
                memcpy(key_value->str, drec_field.data, std::min<int>(drec_field.size, 128));
                advance_value_ptr(key_value, 128);
                break;
            default: assert(0);
            }
            break;

        case ViewFieldKind::SourceIPAddressKey:
            if (fds_drec_find(&drec, IPFIX::iana, IPFIX::sourceIPv4Address, &drec_field) != FDS_EOC) {
                key_value->ip = make_ipv4_address(drec_field.data);
            } else if (fds_drec_find(&drec, IPFIX::iana, IPFIX::sourceIPv6Address, &drec_field) != FDS_EOC) {
                key_value->ip = make_ipv6_address(drec_field.data);
            } else {
                return 0;
            }
            advance_value_ptr(key_value, sizeof(key_value->ip));
            break;

        case ViewFieldKind::DestinationIPAddressKey:
            if (fds_drec_find(&drec, IPFIX::iana, IPFIX::destinationIPv4Address, &drec_field) != FDS_EOC) {
                key_value->ip = make_ipv4_address(drec_field.data);
            } else if (fds_drec_find(&drec, IPFIX::iana, IPFIX::destinationIPv6Address, &drec_field) != FDS_EOC) {
                key_value->ip = make_ipv6_address(drec_field.data);
            } else {
                return 0;
            }
            advance_value_ptr(key_value, sizeof(key_value->ip));
            break;

        case ViewFieldKind::BidirectionalIPAddressKey:
            switch (direction) {
            case Direction::Out:
                if (fds_drec_find(&drec, IPFIX::iana, IPFIX::sourceIPv4Address, &drec_field) != FDS_EOC) {
                    key_value->ip = make_ipv4_address(drec_field.data);
                } else if (fds_drec_find(&drec, IPFIX::iana, IPFIX::sourceIPv6Address, &drec_field) != FDS_EOC) {
                    key_value->ip = make_ipv6_address(drec_field.data);
                } else {
                    return 0;
                }
                break;
            case Direction::In:
                if (fds_drec_find(&drec, IPFIX::iana, IPFIX::destinationIPv4Address, &drec_field) != FDS_EOC) {
                    key_value->ip = make_ipv4_address(drec_field.data);
                } else if (fds_drec_find(&drec, IPFIX::iana, IPFIX::destinationIPv6Address, &drec_field) != FDS_EOC) {
                    key_value->ip = make_ipv6_address(drec_field.data);
                } else {
                    return 0;
                }
                break;
            default: assert(0);
            }
            advance_value_ptr(key_value, sizeof(key_value->ip));
            break;

        case ViewFieldKind::BidirectionalPortKey:
            switch (direction) {
            case Direction::Out:
                if (fds_drec_find(&drec, IPFIX::iana, IPFIX::sourceTransportPort, &drec_field) != FDS_EOC) {
                    key_value->u16 = get_uint(drec_field);
                } else {
                    return 0;
                }
                break;
            case Direction::In:
                if (fds_drec_find(&drec, IPFIX::iana, IPFIX::destinationTransportPort, &drec_field) != FDS_EOC) {
                    key_value->u16 = get_uint(drec_field);
                } else {
                    return 0;
                }
                break;
            default: assert(0);
            }
            advance_value_ptr(key_value, sizeof(key_value->u16));
            break;

        case ViewFieldKind::IPv4SubnetKey:
            if (fds_drec_find(&drec, view_field.pen, view_field.id, &drec_field) == FDS_EOC) {
                return 0;
            }
            memcpy_bits(key_value->ipv4, drec_field.data, view_field.extra.prefix_length);
            advance_value_ptr(key_value, sizeof(key_value->ipv4));
            break;

        case ViewFieldKind::IPv6SubnetKey:
            if (fds_drec_find(&drec, view_field.pen, view_field.id, &drec_field) == FDS_EOC) {
                return 0;
            }
            memcpy_bits(key_value->ipv6, drec_field.data, view_field.extra.prefix_length);
            advance_value_ptr(key_value, sizeof(key_value->ipv6));
            break;

        case ViewFieldKind::BidirectionalIPv4SubnetKey:
            switch (direction) {
            case Direction::Out:
                if (fds_drec_find(&drec, IPFIX::iana, IPFIX::sourceIPv4Address, &drec_field) == FDS_EOC) {
                    return 0;
                }
                break;
            case Direction::In:
                if (fds_drec_find(&drec, IPFIX::iana, IPFIX::destinationIPv4Address, &drec_field) == FDS_EOC) {
                    return 0;
                }
                break;
            default: assert(0);
            }
            memcpy_bits(key_value->ipv4, drec_field.data, view_field.extra.prefix_length);
            advance_value_ptr(key_value, sizeof(key_value->ipv4));
            break;

        case ViewFieldKind::BidirectionalIPv6SubnetKey:
            switch (direction) {
            case Direction::Out:
                if (fds_drec_find(&drec, IPFIX::iana, IPFIX::sourceIPv6Address, &drec_field) == FDS_EOC) {
                    return 0;
                }
                break;
            case Direction::In:
                if (fds_drec_find(&drec, IPFIX::iana, IPFIX::destinationIPv6Address, &drec_field) == FDS_EOC) {
                    return 0;
                }
                break;
            default: assert(0);
            }
            memcpy_bits(key_value->ipv6, drec_field.data, view_field.extra.prefix_length);
            advance_value_ptr(key_value, sizeof(key_value->ipv4));
            break;

        default: assert(0);
        }

    }

    return 1;
}

static void
aggregate_value(const ViewField &aggregate_field, fds_drec &drec, ViewValue *&value, Direction direction)
{
    if (aggregate_field.direction != Direction::None && direction != aggregate_field.direction) {
        advance_value_ptr(value, aggregate_field.size);
        return;
    }

    fds_drec_field drec_field;

    switch (aggregate_field.kind) {

    case ViewFieldKind::SumAggregate:
        if (fds_drec_find(&drec, aggregate_field.pen, aggregate_field.id, &drec_field) == FDS_EOC) {
            return;
        }

        switch (aggregate_field.data_type) {
        case DataType::Unsigned64:
            value->u64 += get_uint(drec_field);
            advance_value_ptr(value, sizeof(value->u64));
            break;
        case DataType::Signed64:
            value->i64 += get_int(drec_field);
            advance_value_ptr(value, sizeof(value->i64));
            break;
        default: assert(0);
        }
        break;

    case ViewFieldKind::FlowCount:
        value->u64++;
        advance_value_ptr(value, sizeof(value->u64));
        break;

    default: assert(0);

    }
}

Aggregator::Aggregator(ViewDefinition view_def) :
    m_view_def(view_def),
    m_table(view_def.keys_size, view_def.values_size)
{
}

void
Aggregator::process_record(fds_drec &drec)
{
    if (m_view_def.bidirectional) {
        aggregate(drec, Direction::In);
        aggregate(drec, Direction::Out);
    } else {
        aggregate(drec, Direction::None);
    }
}

void
Aggregator::aggregate(fds_drec &drec, Direction direction)
{
    constexpr std::size_t key_buffer_size = 1024;
    assert(m_view_def.keys_size <= key_buffer_size);

    static uint8_t key_buffer[key_buffer_size];

    if (!build_key(m_view_def, drec, key_buffer, direction)) {
        return;
    }

    AggregateRecord *record;
    m_table.lookup(key_buffer, record);

    ViewValue *value = reinterpret_cast<ViewValue *>(record->data + m_view_def.keys_size);
    for (const auto &aggregate_field : m_view_def.value_fields) {
        aggregate_value(aggregate_field, drec, value, direction);
    }
}

// void
// Aggregator::print_debug_info()
// {
//     std::size_t n_total_records = 0;
//     std::size_t max_records_in_bucket = 0;
//     std::size_t min_records_in_bucket = 0;
//     std::size_t avg_records_in_bucket = 0;
//     for (const auto bucket : m_buckets) {
//         AggregateRecord *rec = bucket;
//         std::size_t n_records = 0;
//         while (rec) {
//             n_records++;
//             rec = rec->next;
//         }
//         n_total_records += n_records;
//         if (n_records > max_records_in_bucket) {
//             max_records_in_bucket = n_records;
//         }
//         if (n_records < min_records_in_bucket) {
//             min_records_in_bucket = n_records;
//         }
//         std::cout << n_records << "\n";
//     }
//     avg_records_in_bucket = n_total_records / m_buckets_count;

//     std::cout << "Total records: " << n_total_records << "\n";
//     std::cout << "Max in bucket: " << max_records_in_bucket << "\n";
//     std::cout << "Min in bucket: " << min_records_in_bucket << "\n";
//     std::cout << "Avg in bucket: " << avg_records_in_bucket << "\n";
// }

