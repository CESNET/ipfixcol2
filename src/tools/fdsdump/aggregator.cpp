#define XXH_INLINE_ALL

#include "aggregator.hpp"
#include "informationelements.hpp"
#include "xxhash.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include "binaryheap.hpp"

static IPAddress
make_ipv4_address(uint8_t *address)
{
    IPAddress ip = {};
    ip.length = 4;
    std::memcpy(ip.address, address, 4);
    return ip;
}

static IPAddress
make_ipv6_address(uint8_t *address)
{
    IPAddress ip = {};
    ip.length = 16;
    std::memcpy(ip.address, address, 16);
    return ip;
}

static void
memcpy_bits(uint8_t *dst, uint8_t *src, unsigned int n_bits)
{
    unsigned int n_bytes = (n_bits + 7) / 8;
    unsigned int rem_bits = 8 - (n_bits % 8);
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

static uint64_t
get_datetime(fds_drec_field &field)
{
    uint64_t tmp;
    int rc = fds_get_datetime_lp_be(field.data, field.size, field.info->def->data_type, &tmp);
    assert(rc == FDS_OK);
    return tmp;
}

static void
init_zeroed_values(const ViewDefinition &view_def, uint8_t *values)
{
    ViewValue *value = reinterpret_cast<ViewValue *>(values);

    for (const auto &field : view_def.value_fields) {

        if (field.kind == ViewFieldKind::MinAggregate) {
            switch (field.data_type) {
            case DataType::Unsigned8:
                value->u8 = UINT8_MAX;
                break;
            case DataType::Unsigned16:
                value->u16 = UINT16_MAX;
                break;
            case DataType::Unsigned32:
                value->u32 = UINT32_MAX;
                break;
            case DataType::Unsigned64:
                value->u64 = UINT64_MAX;
                break;
            case DataType::Signed8:
                value->i8 = INT8_MAX;
                break;
            case DataType::Signed16:
                value->i16 = INT16_MAX;
                break;
            case DataType::Signed32:
                value->i32 = INT32_MAX;
                break;
            case DataType::Signed64:
                value->i64 = INT64_MAX;
                break;
            case DataType::DateTime:
                value->ts_millisecs = UINT64_MAX;
                break;
            default: assert(0);
            }

        } else if (field.kind == ViewFieldKind::MaxAggregate) {
            switch (field.data_type) {
            case DataType::Unsigned8:
            case DataType::Unsigned16:
            case DataType::Unsigned32:
            case DataType::Unsigned64:
            case DataType::DateTime:
                break;
            case DataType::Signed8:
                value->i8 = INT8_MIN;
                break;
            case DataType::Signed16:
                value->i16 = INT16_MIN;
                break;
            case DataType::Signed32:
                value->i32 = INT32_MIN;
                break;
            case DataType::Signed64:
                value->i64 = INT64_MIN;
                break;
            default: assert(0);
            }
        }

        advance_value_ptr(value, field.size);
    }
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
                break;
            case DataType::Unsigned16:
                key_value->u16 = get_uint(drec_field);
                break;
            case DataType::Unsigned32:
                key_value->u32 = get_uint(drec_field);
                break;
            case DataType::Unsigned64:
                key_value->u64 = get_uint(drec_field);
                break;
            case DataType::Signed8:
                key_value->i8 = get_int(drec_field);
                break;
            case DataType::Signed16:
                key_value->i16 = get_int(drec_field);
                break;
            case DataType::Signed32:
                key_value->i32 = get_int(drec_field);
                break;
            case DataType::Signed64:
                key_value->i64 = get_int(drec_field);
                break;
            case DataType::String128B:
                memset(key_value->str, 0, 128);
                memcpy(key_value->str, drec_field.data, std::min<int>(drec_field.size, 128));
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
            break;

        case ViewFieldKind::DestinationIPAddressKey:
            if (fds_drec_find(&drec, IPFIX::iana, IPFIX::destinationIPv4Address, &drec_field) != FDS_EOC) {
                key_value->ip = make_ipv4_address(drec_field.data);
            } else if (fds_drec_find(&drec, IPFIX::iana, IPFIX::destinationIPv6Address, &drec_field) != FDS_EOC) {
                key_value->ip = make_ipv6_address(drec_field.data);
            } else {
                return 0;
            }
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
            break;

        case ViewFieldKind::IPv4SubnetKey:
            if (fds_drec_find(&drec, view_field.pen, view_field.id, &drec_field) == FDS_EOC) {
                return 0;
            }
            memcpy_bits(key_value->ipv4, drec_field.data, view_field.extra.prefix_length);
            break;

        case ViewFieldKind::IPv6SubnetKey:
            if (fds_drec_find(&drec, view_field.pen, view_field.id, &drec_field) == FDS_EOC) {
                return 0;
            }
            memcpy_bits(key_value->ipv6, drec_field.data, view_field.extra.prefix_length);
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
            break;

        default: assert(0);
        }

        advance_value_ptr(key_value, view_field.size);
    }

    return 1;
}

static void
merge_value(const ViewField &aggregate_field, ViewValue *value, ViewValue *other_value)
{
    switch (aggregate_field.kind) {

    case ViewFieldKind::SumAggregate:
        switch (aggregate_field.data_type) {
        case DataType::Unsigned64:
            value->u64 += other_value->u64;
            break;
        case DataType::Signed64:
            value->i64 += other_value->i64;
            break;
        default: assert(0);
        }

        break;

    case ViewFieldKind::MinAggregate:
        switch (aggregate_field.data_type) {
        case DataType::Unsigned8:
            value->u8 = std::min<uint8_t>(other_value->u8, value->u8);
            break;
        case DataType::Unsigned16:
            value->u16 = std::min<uint16_t>(other_value->u16, value->u16);
            break;
        case DataType::Unsigned32:
            value->u32 = std::min<uint32_t>(other_value->u32, value->u32);
            break;
        case DataType::Unsigned64:
            value->u64 = std::min<uint64_t>(other_value->u64, value->u64);
            break;
        case DataType::Signed8:
            value->i8 = std::min<int8_t>(other_value->i8, value->i8);
            break;
        case DataType::Signed16:
            value->i16 = std::min<int16_t>(other_value->i16, value->i16);
            break;
        case DataType::Signed32:
            value->i32 = std::min<int32_t>(other_value->i32, value->i32);
            break;
        case DataType::Signed64:
            value->i64 = std::min<int64_t>(other_value->i64, value->i64);
            break;
        case DataType::DateTime:
            value->ts_millisecs = std::min<uint64_t>(other_value->ts_millisecs, value->ts_millisecs);
            break;
        default: assert(0);
        }

        break;

    case ViewFieldKind::MaxAggregate:
        switch (aggregate_field.data_type) {
        case DataType::Unsigned8:
            value->u8 = std::max<uint8_t>(other_value->u8, value->u8);
            break;
        case DataType::Unsigned16:
            value->u16 = std::max<uint16_t>(other_value->u16, value->u16);
            break;
        case DataType::Unsigned32:
            value->u32 = std::max<uint32_t>(other_value->u32, value->u32);
            break;
        case DataType::Unsigned64:
            value->u64 = std::max<uint64_t>(other_value->u64, value->u64);
            break;
        case DataType::Signed8:
            value->i8 = std::max<int8_t>(other_value->i8, value->i8);
            break;
        case DataType::Signed16:
            value->i16 = std::max<int16_t>(other_value->i16, value->i16);
            break;
        case DataType::Signed32:
            value->i32 = std::max<int32_t>(other_value->i32, value->i32);
            break;
        case DataType::Signed64:
            value->i64 = std::max<int64_t>(other_value->i64, value->i64);
            break;
        case DataType::DateTime:
            value->ts_millisecs = std::max<uint64_t>(other_value->ts_millisecs, value->ts_millisecs);
            break;
        default: assert(0);
        }

        break;

    case ViewFieldKind::CountAggregate:
        value->u64 += other_value->u64;
        break;

    default: assert(0);

    }
}

static void
aggregate_value(const ViewField &aggregate_field, fds_drec &drec, ViewValue *value, Direction direction)
{
    if (aggregate_field.direction != Direction::Unassigned && direction != aggregate_field.direction) {
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
            break;
        case DataType::Signed64:
            value->i64 += get_int(drec_field);
            break;
        default: assert(0);
        }

        break;

    case ViewFieldKind::MinAggregate:
        if (fds_drec_find(&drec, aggregate_field.pen, aggregate_field.id, &drec_field) == FDS_EOC) {
            return;
        }

        switch (aggregate_field.data_type) {
        case DataType::Unsigned8:
            value->u8 = std::min<uint8_t>(get_uint(drec_field), value->u8);
            break;
        case DataType::Unsigned16:
            value->u16 = std::min<uint16_t>(get_uint(drec_field), value->u16);
            break;
        case DataType::Unsigned32:
            value->u32 = std::min<uint32_t>(get_uint(drec_field), value->u32);
            break;
        case DataType::Unsigned64:
            value->u64 = std::min<uint64_t>(get_uint(drec_field), value->u64);
            break;
        case DataType::Signed8:
            value->i8 = std::min<int8_t>(get_int(drec_field), value->i8);
            break;
        case DataType::Signed16:
            value->i16 = std::min<int16_t>(get_int(drec_field), value->i16);
            break;
        case DataType::Signed32:
            value->i32 = std::min<int32_t>(get_int(drec_field), value->i32);
            break;
        case DataType::Signed64:
            value->i64 = std::min<int64_t>(get_int(drec_field), value->i64);
            break;
        case DataType::DateTime:
            value->ts_millisecs = std::min<uint64_t>(get_datetime(drec_field), value->ts_millisecs);
            break;
        default: assert(0);
        }

        break;

    case ViewFieldKind::MaxAggregate:
        if (fds_drec_find(&drec, aggregate_field.pen, aggregate_field.id, &drec_field) == FDS_EOC) {
            return;
        }

        switch (aggregate_field.data_type) {
        case DataType::Unsigned8:
            value->u8 = std::max<uint8_t>(get_uint(drec_field), value->u8);
            break;
        case DataType::Unsigned16:
            value->u16 = std::max<uint16_t>(get_uint(drec_field), value->u16);
            break;
        case DataType::Unsigned32:
            value->u32 = std::max<uint32_t>(get_uint(drec_field), value->u32);
            break;
        case DataType::Unsigned64:
            value->u64 = std::max<uint64_t>(get_uint(drec_field), value->u64);
            break;
        case DataType::Signed8:
            value->i8 = std::max<int8_t>(get_int(drec_field), value->i8);
            break;
        case DataType::Signed16:
            value->i16 = std::max<int16_t>(get_int(drec_field), value->i16);
            break;
        case DataType::Signed32:
            value->i32 = std::max<int32_t>(get_int(drec_field), value->i32);
            break;
        case DataType::Signed64:
            value->i64 = std::max<int64_t>(get_int(drec_field), value->i64);
            break;
        case DataType::DateTime:
            value->ts_millisecs = std::max<uint64_t>(get_datetime(drec_field), value->ts_millisecs);
            break;
        default: assert(0);
        }

        break;

    case ViewFieldKind::CountAggregate:
        value->u64++;
        break;

    default: assert(0);

    }
}

Aggregator::Aggregator(ViewDefinition view_def) :
    m_view_def(view_def),
    m_table(view_def.keys_size, view_def.values_size),
    m_key_buffer(view_def.keys_size)
{
}

void
Aggregator::process_record(fds_drec &drec)
{
    if (m_view_def.bidirectional) {
        aggregate(drec, Direction::In);
        aggregate(drec, Direction::Out);
    } else {
        aggregate(drec, Direction::Unassigned);
    }
}

void
Aggregator::aggregate(fds_drec &drec, Direction direction)
{
    if (!build_key(m_view_def, drec, &m_key_buffer[0], direction)) {
        return;
    }

    AggregateRecord *record;
    int rc = m_table.lookup(&m_key_buffer[0], record);

    if (rc == 1) {
        init_zeroed_values(m_view_def, record->data + m_view_def.keys_size);
    }

    ViewValue *value = reinterpret_cast<ViewValue *>(record->data + m_view_def.keys_size);
    for (const auto &aggregate_field : m_view_def.value_fields) {
        aggregate_value(aggregate_field, drec, value, direction);
        advance_value_ptr(value, aggregate_field.size);
    }
}

void
Aggregator::merge(Aggregator &other)
{
    for (auto *other_record : other.records()) {
        AggregateRecord *record;
        int rc = m_table.lookup(other_record->data, record);

        if (rc == 1) {
            //TODO: this copy is unnecessary, we could just take the already allocated record from the other table instead
            memcpy(record->data, other_record->data, m_view_def.keys_size + m_view_def.values_size);

        } else {
            ViewValue *value = (ViewValue *) (record->data + m_view_def.keys_size);
            ViewValue *other_value = (ViewValue *) (other_record->data+ m_view_def.keys_size);

            for (const auto &aggregate_field : m_view_def.value_fields) {
                merge_value(aggregate_field, value, other_value);
                advance_value_ptr(value, aggregate_field.size);
                advance_value_ptr(other_value, aggregate_field.size);
            }

        }
    }
}

void
Aggregator::make_top_n(size_t n, CompareFn compare_fn)
{
    auto &recs = records();

    if (recs.size() <= n) {
        std::sort(recs.begin(), recs.end(), compare_fn);
        return;
    }

    auto records_it = recs.begin();
    BinaryHeap<AggregateRecord *, CompareFn> heap(compare_fn);

    size_t i = 0;
    for (auto rec : recs) {
        if (i == n) {
            break;
        }
        //printf("pushing %p\n", rec);
        heap.push(rec);
        i++;
    }

    for (auto it = recs.begin() + n; it != recs.end(); it++) {
        AggregateRecord *rec = *it;
        heap.push_pop(rec);
    }


    assert(n == heap.size());

    recs.resize(n);

    for (auto it = recs.rbegin(); it != recs.rend(); it++) {
        *it = heap.pop();
    }
}



