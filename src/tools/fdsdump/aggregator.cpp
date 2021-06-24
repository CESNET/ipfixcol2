#define XXH_INLINE_ALL

#include "aggregator.hpp"
#include "information_elements.hpp"
#include "xxhash.h"
#include <cstring>


static ip_address_s
make_ipv4_address(uint8_t *address)
{
    ip_address_s a = {};
    a.length = 4;
    std::memcpy(a.address, address, 4);
    return a;
}

static ip_address_s
make_ipv6_address(uint8_t *address)
{
    ip_address_s a = {};
    a.length = 16;
    std::memcpy(a.address, address, 16);
    return a;
}

Aggregator::Aggregator()
{
    for (auto &bucket : m_buckets) {
        bucket = nullptr;
    }
}

void
Aggregator::process_record(fds_drec &drec)
{
    aggregate_record_s::key_s key = {};
    fds_drec_field field;
    uint64_t tmp;

    auto fetch = [&](uint16_t id) -> bool {
        return fds_drec_find(&drec, IPFIX::iana, id, &field) != FDS_EOC;
    };

    if (fetch(IPFIX::sourceIPv4Address)) {
        key.src_ip = make_ipv4_address(field.data);
    } else if (fetch(IPFIX::sourceIPv6Address)) {
        key.src_ip = make_ipv6_address(field.data);
    } else {
        return;
    }

    if (fetch(IPFIX::destinationIPv4Address)) {
        key.dst_ip = make_ipv4_address(field.data);
    } else if (fetch(IPFIX::destinationIPv6Address)) {
        key.dst_ip = make_ipv6_address(field.data);
    } else {
        return;
    }

    if (!fetch(IPFIX::sourceTransportPort)) {
        return;
    }
    fds_get_uint_be(field.data, field.size, &tmp);
    key.src_port = tmp;

    if (!fetch(IPFIX::destinationTransportPort)) {
        return;
    }
    fds_get_uint_be(field.data, field.size, &tmp);
    key.dst_port = tmp;

    if (!fetch(IPFIX::protocolIdentifier)) {
        return;
    }

    key.protocol = *field.data;

    auto hash = XXH3_64bits(reinterpret_cast<uint8_t *>(&key), sizeof(key));
    std::size_t bucket_index = hash % BUCKETS_COUNT;

    aggregate_record_s **arec = &m_buckets[bucket_index];

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
        *arec = new aggregate_record_s{};
        m_records.push_back(*arec);
        std::memcpy(reinterpret_cast<uint8_t *>(&((*arec)->key)), reinterpret_cast<uint8_t *>(&key), sizeof(key));
    }

    if (fetch(IPFIX::octetDeltaCount)) {
        fds_get_uint_be(field.data, field.size, &tmp);
        (*arec)->bytes += tmp;
    }

    if (fetch(IPFIX::packetDeltaCount)) {
        fds_get_uint_be(field.data, field.size, &tmp);
        (*arec)->packets += tmp;
    }

    if (fetch(IPFIX::deltaFlowCount)) {
        fds_get_uint_be(field.data, field.size, &tmp);
        (*arec)->flows += tmp;
    }
}