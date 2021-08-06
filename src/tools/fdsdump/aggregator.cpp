#define XXH_INLINE_ALL

#include "aggregator.hpp"
#include "informationelements.hpp"
#include "xxhash.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include "binaryheap.hpp"
#include "tableprinter.hpp"

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
init_value(const ViewField &field, ViewValue &value)
{
    switch (field.kind) {
    case ViewFieldKind::MinAggregate:

        switch (field.data_type) {
        case DataType::Unsigned8:
            value.u8 = UINT8_MAX;
            break;
        case DataType::Unsigned16:
            value.u16 = UINT16_MAX;
            break;
        case DataType::Unsigned32:
            value.u32 = UINT32_MAX;
            break;
        case DataType::Unsigned64:
            value.u64 = UINT64_MAX;
            break;
        case DataType::Signed8:
            value.i8 = INT8_MAX;
            break;
        case DataType::Signed16:
            value.i16 = INT16_MAX;
            break;
        case DataType::Signed32:
            value.i32 = INT32_MAX;
            break;
        case DataType::Signed64:
            value.i64 = INT64_MAX;
            break;
        case DataType::DateTime:
            value.ts_millisecs = UINT64_MAX;
            break;
        default: assert(0);
        }

        break;

    case ViewFieldKind::MaxAggregate:

        switch (field.data_type) {
        case DataType::Unsigned8:
        case DataType::Unsigned16:
        case DataType::Unsigned32:
        case DataType::Unsigned64:
        case DataType::DateTime:
            memset((uint8_t *) &value, 0, field.size);
            break;
        case DataType::Signed8:
            value.i8 = INT8_MIN;
            break;
        case DataType::Signed16:
            value.i16 = INT16_MIN;
            break;
        case DataType::Signed32:
            value.i32 = INT32_MIN;
            break;
        case DataType::Signed64:
            value.i64 = INT64_MIN;
            break;
        default: assert(0);
        }

        break;

    default:
        memset((uint8_t *) &value, 0, field.size);
        break;
    }
}

static void
init_values(const ViewDefinition &view_def, uint8_t *values)
{
    ViewValue *value = reinterpret_cast<ViewValue *>(values);

    for (const auto &field : view_def.value_fields) {
        init_value(field, *value);
        advance_value_ptr(value, field.size);
    }
}

static void
merge_value(const ViewField &aggregate_field, ViewValue &value, ViewValue &other_value)
{
    switch (aggregate_field.kind) {

    case ViewFieldKind::SumAggregate:
        switch (aggregate_field.data_type) {
        case DataType::Unsigned64:
            value.u64 += other_value.u64;
            break;
        case DataType::Signed64:
            value.i64 += other_value.i64;
            break;
        default: assert(0);
        }

        break;

    case ViewFieldKind::MinAggregate:
        switch (aggregate_field.data_type) {
        case DataType::Unsigned8:
            value.u8 = std::min<uint8_t>(other_value.u8, value.u8);
            break;
        case DataType::Unsigned16:
            value.u16 = std::min<uint16_t>(other_value.u16, value.u16);
            break;
        case DataType::Unsigned32:
            value.u32 = std::min<uint32_t>(other_value.u32, value.u32);
            break;
        case DataType::Unsigned64:
            value.u64 = std::min<uint64_t>(other_value.u64, value.u64);
            break;
        case DataType::Signed8:
            value.i8 = std::min<int8_t>(other_value.i8, value.i8);
            break;
        case DataType::Signed16:
            value.i16 = std::min<int16_t>(other_value.i16, value.i16);
            break;
        case DataType::Signed32:
            value.i32 = std::min<int32_t>(other_value.i32, value.i32);
            break;
        case DataType::Signed64:
            value.i64 = std::min<int64_t>(other_value.i64, value.i64);
            break;
        case DataType::DateTime:
            value.ts_millisecs = std::min<uint64_t>(other_value.ts_millisecs, value.ts_millisecs);
            break;
        default: assert(0);
        }

        break;

    case ViewFieldKind::MaxAggregate:
        switch (aggregate_field.data_type) {
        case DataType::Unsigned8:
            value.u8 = std::max<uint8_t>(other_value.u8, value.u8);
            break;
        case DataType::Unsigned16:
            value.u16 = std::max<uint16_t>(other_value.u16, value.u16);
            break;
        case DataType::Unsigned32:
            value.u32 = std::max<uint32_t>(other_value.u32, value.u32);
            break;
        case DataType::Unsigned64:
            value.u64 = std::max<uint64_t>(other_value.u64, value.u64);
            break;
        case DataType::Signed8:
            value.i8 = std::max<int8_t>(other_value.i8, value.i8);
            break;
        case DataType::Signed16:
            value.i16 = std::max<int16_t>(other_value.i16, value.i16);
            break;
        case DataType::Signed32:
            value.i32 = std::max<int32_t>(other_value.i32, value.i32);
            break;
        case DataType::Signed64:
            value.i64 = std::max<int64_t>(other_value.i64, value.i64);
            break;
        case DataType::DateTime:
            value.ts_millisecs = std::max<uint64_t>(other_value.ts_millisecs, value.ts_millisecs);
            break;
        default: assert(0);
        }

        break;

    case ViewFieldKind::CountAggregate:
        value.u64 += other_value.u64;
        break;

    default: assert(0);

    }
}

static void
merge_records(const ViewDefinition &def, uint8_t *record, uint8_t *other_record)
{
    ViewValue *value = (ViewValue *) (record + def.keys_size);
    ViewValue *other_value = (ViewValue *) (other_record + def.keys_size);

    for (const auto &aggregate_field : def.value_fields) {
        merge_value(aggregate_field, *value, *other_value);
        advance_value_ptr(value, aggregate_field.size);
        advance_value_ptr(other_value, aggregate_field.size);
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

    uint8_t *record;

    if (!m_table.find_or_create(&m_key_buffer[0], record)) {
        init_values(m_view_def, record + m_view_def.keys_size);
        m_items.push_back(record);
    }

    ViewValue *value = reinterpret_cast<ViewValue *>(record + m_view_def.keys_size);
    for (const auto &aggregate_field : m_view_def.value_fields) {
        aggregate_value(aggregate_field, drec, value, direction);
        advance_value_ptr(value, aggregate_field.size);
    }
}

void
Aggregator::merge(Aggregator &other)
{
    for (uint8_t *other_record : other.m_items) {
        uint8_t *record;

        if (!m_table.find_or_create(other_record, record)) {
            //TODO: this copy is unnecessary, we could just take the already allocated record from the other table instead
            memcpy(record, other_record, m_view_def.keys_size + m_view_def.values_size);
            m_items.push_back(record);

        } else {
            merge_records(m_view_def, record, other_record);
/*
            ViewValue *value = (ViewValue *) (record + m_view_def.keys_size);
            ViewValue *other_value = (ViewValue *) (other_record->data+ m_view_def.keys_size);

            for (const auto &aggregate_field : m_view_def.value_fields) {
                merge_value(aggregate_field, *value, *other_value);
                advance_value_ptr(value, aggregate_field.size);
                advance_value_ptr(other_value, aggregate_field.size);
            }
*/

        }
    }
}

void
Aggregator::make_top_n(size_t n, CompareFn compare_fn)
{
    auto &recs = m_items;

    if (recs.size() <= n) {
        std::sort(recs.begin(), recs.end(), compare_fn);
        return;
    }

    auto records_it = recs.begin();
    BinaryHeap<uint8_t *, CompareFn> heap(compare_fn);

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
        uint8_t *rec = *it;
        heap.push_pop(rec);
    }


    assert(n == heap.size());

    recs.resize(n);

    for (auto it = recs.rbegin(); it != recs.rend(); it++) {
        *it = heap.pop();
    }
}

int
compare_values(const ViewField &field, const ViewValue &a, const ViewValue &b)
{
    switch (field.data_type) {
    case DataType::DateTime:
    case DataType::Unsigned64:
        return a.u64 > b.u64 ? 1 : (a.u64 < b.u64 ? -1 : 0);

    case DataType::Signed64:
        return a.i64 > b.i64 ? 1 : (a.i64 < b.i64 ? -1 : 0);

    default:
        assert(0);
    }
}

int
compare_records(SortField sort_field, const ViewDefinition &def, uint8_t *record, uint8_t *other_record)
{
    int result = compare_values(*sort_field.field,
                                *(ViewValue *) (record + sort_field.field->offset),
                                *(ViewValue *) (other_record + sort_field.field->offset));

    if (sort_field.ascending) {
        result = -result;
    }

    return result;
}


int
compare_records(const std::vector<SortField> &sort_fields, const ViewDefinition &def, uint8_t *record, uint8_t *other_record)
{
    int result;

    for (auto sort_field : sort_fields) {
        result = compare_values(*sort_field.field,
                                *(ViewValue *) (record + sort_field.field->offset),
                                *(ViewValue *) (other_record + sort_field.field->offset));

        if (result != 0) {
            if (sort_field.ascending) {
                result = -result;
            }
            break;
        }

    }

    return result;
}

static std::vector<uint8_t>
make_empty_record(const ViewDefinition &def)
{
    std::vector<uint8_t> empty_record(def.keys_size + def.values_size);
    init_values(def, &empty_record[0]);
    return empty_record;
}

static bool
merge_index(const ViewDefinition &def, std::vector<Aggregator *> &aggregators, size_t idx, uint8_t *base_record)
{
    bool any = false;

    for (auto *aggregator : aggregators) {
        const auto &records = aggregator->m_items;

        if (idx >= records.size()) {
            continue;
        }

        merge_records(def, base_record, records[idx]);

        any = true;
    }

    return any;
}

static void
merge_corresponding(const ViewDefinition &def, std::vector<Aggregator> &aggregators, uint8_t *base_record)
{
    for (auto &aggregator : aggregators) {
        uint8_t *other_record;

        if (aggregator.m_table.find(base_record, other_record)) {
            merge_records(def, base_record, other_record);
        }
    }
}


std::vector<uint8_t *>
make_top_n(const ViewDefinition &def, std::vector<Aggregator *> &aggregators, size_t n, const std::vector<SortField> &sort_fields)
{
    // Using the threshold algorithm for distributed top-k
    // Records in individual aggregators must be sorted

    std::function<bool(uint8_t *, uint8_t *)> compare;

    if (sort_fields.size() == 1) {
        compare = [&](uint8_t *a, uint8_t *b) -> bool {
            return compare_records(sort_fields[0], def, a, b) > 0;
        };

    } else {
        compare = [&](uint8_t *a, uint8_t *b) -> bool {
            return compare_records(sort_fields, def, a, b) > 0;
        };

    }

    BinaryHeap<uint8_t *, std::function<bool(uint8_t *, uint8_t *)>> heap(compare);
    HashTable seen(def.keys_size, 0);

    size_t idx = 0;

    std::vector<uint8_t> empty_record = make_empty_record(def);

    for (;;) {

        assert(heap.size() <= n);

        if (heap.size() == n) {
            std::vector<uint8_t> threshold = empty_record;

            if (!merge_index(def, aggregators, idx, threshold.data())) {
                break;
            }
/*
            printf("\n");
            printf("Threshold:\n");
            TablePrinter(def).print_record(threshold.data());
            printf("Record %u:\n", idx);
            TablePrinter(def).print_record(aggregators[0]->m_items[idx]);
*/
            if (compare_records(sort_fields, def, heap.top(), threshold.data()) >= 0) {
  /*
                printf("Threshold is %u, heap top is %u\n",
                    *(uint64_t *) (threshold.data() + sort_fields[0].field->offset),
                    *(uint64_t *) (heap.top() + sort_fields[0].field->offset));
            */
                break;
            }
        }

        for (auto *aggregator : aggregators) {
            if (idx >= aggregator->m_items.size()) {
                continue;
            }

            uint8_t *record = aggregator->m_items[idx];
            uint8_t *throwaway;
            if (seen.find_or_create(record, throwaway)) {
                continue;
            }

            for (auto *aggregator_ : aggregators) {
                if (aggregator == aggregator_) {
                    continue;
                }

                uint8_t *other_record;

                if (aggregator_->m_table.find(record, other_record)) {
                    merge_records(def, record, other_record);
                    //TODO: free memory
                }
            }
/*
            printf("Record to push is %u\n",
                *(uint64_t *) (record + sort_fields[0].field->offset));
*/
            if (heap.size() < n) {
                heap.push(record);
                //TODO: free memory
            } else {
                heap.push_pop(record);
            }
        }

        idx++;
    }

    std::vector<uint8_t *> top_records(heap.size());

    size_t i = heap.size();
    while (heap.size() > 0) {
        i--;
        top_records[i] = heap.pop();
    }

    return top_records;
}