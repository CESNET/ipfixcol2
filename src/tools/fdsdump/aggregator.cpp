#define XXH_INLINE_ALL

#include "aggregator.hpp"
#include "information_elements.hpp"
#include "xxhash.h"
#include <cstring>

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

uint64_t
get_uint(fds_drec_field &field)
{
    uint64_t tmp;
    int rc = fds_get_uint_be(field.data, field.size, &tmp);
    assert(rc == FDS_OK);
    return tmp;
}

int64_t
get_int(fds_drec_field &field)
{
    int64_t tmp;
    int rc = fds_get_int_be(field.data, field.size, &tmp);
    assert(rc == FDS_OK);
    return tmp;
}

static void
aggregate_value(const AggregateField &aggregate_field, fds_drec &drec, Value *&value)
{
    fds_drec_field drec_field;

    switch (aggregate_field.kind) {

    case AggregateFieldKind::Sum:
        if (fds_drec_find(&drec, aggregate_field.pen, aggregate_field.id, &drec_field) != FDS_OK) {
            return;
        }

        switch (aggregate_field.data_type) {
        case DataType::Unsigned64:
            value->u64 += get_uint(drec_field);
            advance_value_ptr(value, sizeof(value->u64));
            break;
        case DataType::Signed64:
            value->i64 += get_uint(drec_field);
            advance_value_ptr(value, sizeof(value->i64));
            break;
        default: assert(0);
        }
        break;
    
    case AggregateFieldKind::FlowCount:
        value->u64++;
        advance_value_ptr(value, sizeof(value->u64));
        break;

    default: assert(0);
    
    }
}

Aggregator::Aggregator(AggregateConfig config) : 
    m_config(config), 
    m_buckets{nullptr}
{
}

void
Aggregator::process_record(fds_drec &drec)
{
    AggregateRecord::Key key = {};
    fds_drec_field field;
    uint64_t tmp;

    auto fetch = [&](uint16_t id) -> bool {
        return fds_drec_find(&drec, IPFIX::iana, id, &field) != FDS_EOC;
    };

    if (m_config.key_src_ip) {
        if (fetch(IPFIX::sourceIPv4Address)) {
            key.src_ip = make_ipv4_address(field.data);
        } else if (fetch(IPFIX::sourceIPv6Address)) {
            key.src_ip = make_ipv6_address(field.data);
        } else {
            return;
        }
    }

    if (m_config.key_dst_ip) {
        if (fetch(IPFIX::destinationIPv4Address)) {
            key.dst_ip = make_ipv4_address(field.data);
        } else if (fetch(IPFIX::destinationIPv6Address)) {
            key.dst_ip = make_ipv6_address(field.data);
        } else {
            return;
        }
    }

    if (m_config.key_src_port) {
        if (!fetch(IPFIX::sourceTransportPort)) {
            return;
        }
        fds_get_uint_be(field.data, field.size, &tmp);
        key.src_port = tmp;
    }

    if (m_config.key_dst_port) {
        if (!fetch(IPFIX::destinationTransportPort)) {
            return;
        }
        fds_get_uint_be(field.data, field.size, &tmp);
        key.dst_port = tmp;
    }

    if (m_config.key_protocol) {
        if (!fetch(IPFIX::protocolIdentifier)) {
            return;
        }
        key.protocol = *field.data;
    }

    auto hash = XXH3_64bits(reinterpret_cast<uint8_t *>(&key), sizeof(key));
    std::size_t bucket_index = hash % BUCKETS_COUNT;

    AggregateRecord **arec = &m_buckets[bucket_index];

    while (1) {
        if (*arec == nullptr) {
            break;
        }

        if (std::memcmp(reinterpret_cast<uint8_t *>(&((*arec)->key)), reinterpret_cast<uint8_t *>(&key), sizeof(key)) == 0) {
            break;
        }

        arec = &((*arec)->next);
    }

    if (*arec == nullptr) {
        void *tmp = calloc(1, sizeof(AggregateRecord) + m_config.values_size);
        if (!tmp) {
            throw std::bad_alloc{};
        }
        *arec = static_cast<AggregateRecord *>(tmp);
        m_records.push_back(*arec);
        std::memcpy(reinterpret_cast<uint8_t *>(&((*arec)->key)), reinterpret_cast<uint8_t *>(&key), sizeof(key));
    }

    Value *value = reinterpret_cast<Value *>((*arec)->values);
    for (const auto &aggregate_field : m_config.value_fields) {
        aggregate_value(aggregate_field, drec, value);
    }
}