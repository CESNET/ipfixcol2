/**
 * @file
 * @author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * @brief Aggregator
 */
#define XXH_INLINE_ALL

#include "aggregator.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <functional>

#include "3rd_party/xxhash/xxhash.h"
#include "common/common.hpp"
#include "common/fieldView.hpp"

#include "binaryHeap.hpp"
#include "informationElements.hpp"
#include "sort.hpp"

static int
fds_drec_find(
    fds_drec *drec,
    uint32_t pen,
    uint16_t id,
    uint16_t flags,
    fds_drec_field *field)
{
    if (flags == 0) {
        return fds_drec_find(drec, pen, id, field);

    } else {
        fds_drec_iter iter;
        fds_drec_iter_init(&iter, drec, flags);

        int ret = fds_drec_iter_find(&iter, pen, id);
        if (ret != FDS_EOC) {
            *field = iter.field;
        }

        assert(ret == FDS_EOC || field->data != nullptr);

        return ret;
    }
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
load_view_value(const ViewField &view_field, fds_drec_field &drec_field, ViewValue &value)
{
    switch (view_field.data_type) {
    case DataType::Unsigned8:
        value.u8 = FieldView(drec_field).as_uint();
        break;
    case DataType::Unsigned16:
        value.u16 = FieldView(drec_field).as_uint();
        break;
    case DataType::Unsigned32:
        value.u32 = FieldView(drec_field).as_uint();
        break;
    case DataType::Unsigned64:
        value.u64 = FieldView(drec_field).as_uint();
        break;
    case DataType::Signed8:
        value.i8 = FieldView(drec_field).as_int();
        break;
    case DataType::Signed16:
        value.i16 = FieldView(drec_field).as_int();
        break;
    case DataType::Signed32:
        value.i32 = FieldView(drec_field).as_int();
        break;
    case DataType::Signed64:
        value.i64 = FieldView(drec_field).as_int();
        break;
    case DataType::String128B:
        memset(value.str, 0, 128);
        memcpy(value.str, drec_field.data, std::min<int>(drec_field.size, 128));
        break;
    case DataType::DateTime:
        value.ts_millisecs = FieldView(drec_field).as_datetime_ms();
        break;
    case DataType::IPv4Address:
        memcpy(&value.ipv4, drec_field.data, sizeof(value.ipv4));
        break;
    case DataType::IPv6Address:
        memcpy(&value.ipv6, drec_field.data, sizeof(value.ipv6));
        break;
    case DataType::IPAddress:
        value.ip.length = drec_field.size;
        memcpy(&value.ip.address, drec_field.data, drec_field.size);
        break;
    case DataType::MacAddress:
        memcpy(value.mac, drec_field.data, 6);
        break;
    default: assert(0);
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

bool
build_key(const ViewDefinition &view_def, fds_drec &drec, uint8_t *key_buffer, ViewDirection direction, uint16_t drec_find_flags)
{
    ViewValue *key_value = reinterpret_cast<ViewValue *>(key_buffer);
    fds_drec_field drec_field;

    for (const auto &view_field : view_def.key_fields) {

        switch (view_field.kind) {
        case ViewFieldKind::VerbatimKey:
            if (fds_drec_find(&drec, view_field.pen, view_field.id, drec_find_flags, &drec_field) == FDS_EOC) {
                return false;
            }

            load_view_value(view_field, drec_field, *key_value);
            break;

        case ViewFieldKind::SourceIPAddressKey:
            if (fds_drec_find(&drec, IPFIX::iana, IPFIX::sourceIPv4Address, drec_find_flags, &drec_field) != FDS_EOC) {
                key_value->ip = make_ipv4_address(drec_field.data);
            } else if (fds_drec_find(&drec, IPFIX::iana, IPFIX::sourceIPv6Address, drec_find_flags, &drec_field) != FDS_EOC) {
                key_value->ip = make_ipv6_address(drec_field.data);
            } else {
                return false;
            }
            break;

        case ViewFieldKind::DestinationIPAddressKey:
            if (fds_drec_find(&drec, IPFIX::iana, IPFIX::destinationIPv4Address, drec_find_flags, &drec_field) != FDS_EOC) {
                key_value->ip = make_ipv4_address(drec_field.data);
            } else if (fds_drec_find(&drec, IPFIX::iana, IPFIX::destinationIPv6Address, drec_find_flags, &drec_field) != FDS_EOC) {
                key_value->ip = make_ipv6_address(drec_field.data);
            } else {
                return false;
            }
            break;

        case ViewFieldKind::BidirectionalIPAddressKey:
            switch (direction) {
            case ViewDirection::Out:
                if (fds_drec_find(&drec, IPFIX::iana, IPFIX::sourceIPv4Address, drec_find_flags, &drec_field) != FDS_EOC) {
                    key_value->ip = make_ipv4_address(drec_field.data);
                } else if (fds_drec_find(&drec, IPFIX::iana, IPFIX::sourceIPv6Address, drec_find_flags, &drec_field) != FDS_EOC) {
                    key_value->ip = make_ipv6_address(drec_field.data);
                } else {
                    return false;
                }
                break;
            case ViewDirection::In:
                if (fds_drec_find(&drec, IPFIX::iana, IPFIX::destinationIPv4Address, drec_find_flags, &drec_field) != FDS_EOC) {
                    key_value->ip = make_ipv4_address(drec_field.data);
                } else if (fds_drec_find(&drec, IPFIX::iana, IPFIX::destinationIPv6Address, drec_find_flags, &drec_field) != FDS_EOC) {
                    key_value->ip = make_ipv6_address(drec_field.data);
                } else {
                    return false;
                }
                break;
            default: assert(0);
            }
            break;

        case ViewFieldKind::BidirectionalPortKey:
            switch (direction) {
            case ViewDirection::Out:
                if (fds_drec_find(&drec, IPFIX::iana, IPFIX::sourceTransportPort, drec_find_flags, &drec_field) != FDS_EOC) {
                    key_value->u16 = FieldView(drec_field).as_uint();
                } else {
                    return false;
                }
                break;
            case ViewDirection::In:
                if (fds_drec_find(&drec, IPFIX::iana, IPFIX::destinationTransportPort, drec_find_flags, &drec_field) != FDS_EOC) {
                    key_value->u16 = FieldView(drec_field).as_uint();
                } else {
                    return false;
                }
                break;
            default: assert(0);
            }
            break;

        case ViewFieldKind::IPv4SubnetKey:
            if (fds_drec_find(&drec, view_field.pen, view_field.id, drec_find_flags, &drec_field) == FDS_EOC) {
                return false;
            }
            memcpy_bits(key_value->ipv4, drec_field.data, view_field.extra.prefix_length);
            break;

        case ViewFieldKind::IPv6SubnetKey:
            if (fds_drec_find(&drec, view_field.pen, view_field.id, drec_find_flags, &drec_field) == FDS_EOC) {
                return false;
            }
            memcpy_bits(key_value->ipv6, drec_field.data, view_field.extra.prefix_length);
            break;

        case ViewFieldKind::BidirectionalIPv4SubnetKey:
            switch (direction) {
            case ViewDirection::Out:
                if (fds_drec_find(&drec, IPFIX::iana, IPFIX::sourceIPv4Address, drec_find_flags, &drec_field) == FDS_EOC) {
                    return false;
                }
                break;
            case ViewDirection::In:
                if (fds_drec_find(&drec, IPFIX::iana, IPFIX::destinationIPv4Address, drec_find_flags, &drec_field) == FDS_EOC) {
                    return false;
                }
                break;
            default: assert(0);
            }
            memcpy_bits(key_value->ipv4, drec_field.data, view_field.extra.prefix_length);
            break;

        case ViewFieldKind::BidirectionalIPv6SubnetKey:
            switch (direction) {
            case ViewDirection::Out:
                if (fds_drec_find(&drec, IPFIX::iana, IPFIX::sourceIPv6Address, drec_find_flags, &drec_field) == FDS_EOC) {
                    return false;
                }
                break;
            case ViewDirection::In:
                if (fds_drec_find(&drec, IPFIX::iana, IPFIX::destinationIPv6Address, drec_find_flags, &drec_field) == FDS_EOC) {
                    return false;
                }
                break;
            default: assert(0);
            }
            memcpy_bits(key_value->ipv6, drec_field.data, view_field.extra.prefix_length);
            break;

        case ViewFieldKind::BiflowDirectionKey:
            if (drec_find_flags & FDS_DREC_BIFLOW_FWD) {
                key_value->u8 = 1;
            } else if (drec_find_flags & FDS_DREC_BIFLOW_REV) {
                key_value->u8 = 2;
            } else {
                //assert(0);
                key_value->u8 = 0;
            }
            break;

        default: assert(0);
        }

        advance_value_ptr(key_value, view_field.size);
    }

    return true;
}


static void
aggregate_value(const ViewField &aggregate_field, fds_drec &drec, ViewValue *value, ViewDirection direction, uint16_t drec_find_flags)
{
    if (aggregate_field.direction != ViewDirection::Unassigned && direction != aggregate_field.direction) {
        return;
    }

    fds_drec_field drec_field;

    switch (aggregate_field.kind) {

    case ViewFieldKind::SumAggregate:
        if (fds_drec_find(&drec, aggregate_field.pen, aggregate_field.id, drec_find_flags, &drec_field) == FDS_EOC) {
            return;
        }

        switch (aggregate_field.data_type) {
        case DataType::Unsigned64:
            value->u64 += FieldView(drec_field).as_uint();
            break;
        case DataType::Signed64:
            value->i64 += FieldView(drec_field).as_int();
            break;
        default: assert(0);
        }

        break;

    case ViewFieldKind::MinAggregate:
        if (fds_drec_find(&drec, aggregate_field.pen, aggregate_field.id, drec_find_flags, &drec_field) == FDS_EOC) {
            return;
        }

        switch (aggregate_field.data_type) {
        case DataType::Unsigned8:
            value->u8 = std::min<uint8_t>(FieldView(drec_field).as_uint(), value->u8);
            break;
        case DataType::Unsigned16:
            value->u16 = std::min<uint16_t>(FieldView(drec_field).as_uint(), value->u16);
            break;
        case DataType::Unsigned32:
            value->u32 = std::min<uint32_t>(FieldView(drec_field).as_uint(), value->u32);
            break;
        case DataType::Unsigned64:
            value->u64 = std::min<uint64_t>(FieldView(drec_field).as_uint(), value->u64);
            break;
        case DataType::Signed8:
            value->i8 = std::min<int8_t>(FieldView(drec_field).as_int(), value->i8);
            break;
        case DataType::Signed16:
            value->i16 = std::min<int16_t>(FieldView(drec_field).as_int(), value->i16);
            break;
        case DataType::Signed32:
            value->i32 = std::min<int32_t>(FieldView(drec_field).as_int(), value->i32);
            break;
        case DataType::Signed64:
            value->i64 = std::min<int64_t>(FieldView(drec_field).as_int(), value->i64);
            break;
        case DataType::DateTime:
            value->ts_millisecs = std::min<uint64_t>(FieldView(drec_field).as_datetime_ms(), value->ts_millisecs);
            break;
        default: assert(0);
        }

        break;

    case ViewFieldKind::MaxAggregate:
        if (fds_drec_find(&drec, aggregate_field.pen, aggregate_field.id, drec_find_flags, &drec_field) == FDS_EOC) {
            return;
        }

        switch (aggregate_field.data_type) {
        case DataType::Unsigned8:
            value->u8 = std::max<uint8_t>(FieldView(drec_field).as_uint(), value->u8);
            break;
        case DataType::Unsigned16:
            value->u16 = std::max<uint16_t>(FieldView(drec_field).as_uint(), value->u16);
            break;
        case DataType::Unsigned32:
            value->u32 = std::max<uint32_t>(FieldView(drec_field).as_uint(), value->u32);
            break;
        case DataType::Unsigned64:
            value->u64 = std::max<uint64_t>(FieldView(drec_field).as_uint(), value->u64);
            break;
        case DataType::Signed8:
            value->i8 = std::max<int8_t>(FieldView(drec_field).as_int(), value->i8);
            break;
        case DataType::Signed16:
            value->i16 = std::max<int16_t>(FieldView(drec_field).as_int(), value->i16);
            break;
        case DataType::Signed32:
            value->i32 = std::max<int32_t>(FieldView(drec_field).as_int(), value->i32);
            break;
        case DataType::Signed64:
            value->i64 = std::max<int64_t>(FieldView(drec_field).as_int(), value->i64);
            break;
        case DataType::DateTime:
            value->ts_millisecs = std::max<uint64_t>(FieldView(drec_field).as_datetime_ms(), value->ts_millisecs);
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
    m_table(view_def.keys_size, view_def.values_size),
    m_view_def(view_def),
    m_key_buffer(view_def.keys_size)
{
}

void
Aggregator::process_record(Flow &flow)
{
    if ((flow.dir & DIRECTION_FWD) != 0) {
        if (!m_view_def.bidirectional) {
            aggregate(flow.rec, ViewDirection::Unassigned, FDS_DREC_BIFLOW_FWD);
        } else {
            aggregate(flow.rec, ViewDirection::In, FDS_DREC_BIFLOW_FWD);
            aggregate(flow.rec, ViewDirection::Out, FDS_DREC_BIFLOW_FWD);
        }
    }

    if ((flow.dir & DIRECTION_REV) != 0) {
        if (!m_view_def.bidirectional) {
            aggregate(flow.rec, ViewDirection::Unassigned, FDS_DREC_BIFLOW_REV);
        } else {
            aggregate(flow.rec, ViewDirection::In, FDS_DREC_BIFLOW_REV);
            aggregate(flow.rec, ViewDirection::Out, FDS_DREC_BIFLOW_REV);
        }
    }
}

void
Aggregator::aggregate(fds_drec &drec, ViewDirection direction, uint16_t drec_find_flags)
{
    if (!build_key(m_view_def, drec, &m_key_buffer[0], direction, drec_find_flags)) {
        return;
    }

    uint8_t *record;

    if (!m_table.find_or_create(&m_key_buffer[0], record)) {
        init_values(m_view_def, record + m_view_def.keys_size);
    }

    ViewValue *value = reinterpret_cast<ViewValue *>(record + m_view_def.keys_size);
    for (const auto &aggregate_field : m_view_def.value_fields) {
        aggregate_value(aggregate_field, drec, value, direction, drec_find_flags);
        advance_value_ptr(value, aggregate_field.size);
    }
}

void
Aggregator::merge(Aggregator &other, unsigned int max_num_items)
{
    unsigned int n = 0;
    for (uint8_t *other_record : other.items()) {
        if (max_num_items != 0 && n == max_num_items) {
            break;
        }

        uint8_t *record;

        if (!m_table.find_or_create(other_record, record)) {
            //TODO: this copy is unnecessary, we could just take the already allocated record from the other table instead
            memcpy(record, other_record, m_view_def.keys_size + m_view_def.values_size);
        } else {
            merge_records(m_view_def, record, other_record);
        }

        n++;
    }
}
