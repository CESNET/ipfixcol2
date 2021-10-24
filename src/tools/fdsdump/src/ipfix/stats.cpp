#include "ipfix/stats.hpp"
#include "ipfix/util.hpp"
#include "ipfix/informationelements.hpp"
#include <inttypes.h>

constexpr uint16_t PROTOCOL_ICMP = 1;
constexpr uint16_t PROTOCOL_TCP = 6;
constexpr uint16_t PROTOCOL_UDP = 17;

static bool field_is(const fds_drec_field &field, uint16_t id, uint32_t pen = IPFIX::iana) {
    return field.info->id == id && field.info->en == pen;
}

void
Stats::process_record(fds_drec &drec)
{
    int protocol = -1;
    fds_drec_field field;
    if (fds_drec_find(&drec, IPFIX::iana, IPFIX::protocolIdentifier, &field) != FDS_EOC) {
        protocol = (int) get_uint(field);
    }

    switch (protocol) {
    case PROTOCOL_ICMP: m_flows_icmp += 1; break;
    case PROTOCOL_TCP:  m_flows_tcp += 1; break;
    case PROTOCOL_UDP:  m_flows_udp += 1; break;
    default:            m_flows_other += 1; break;
    }
    m_flows += 1;

    fds_drec_iter it;
    fds_drec_iter_init(&it, &drec, 0);
    int rc;
    while (fds_drec_iter_next(&it) != FDS_EOC) {
        /* Is this the one we should use for flows?
        if (field_is(it.field, IPFIX::deltaFlowCount)) {
            uint64_t value = get_uint(it.field);
            switch (protocol) {
            case PROTOCOL_ICMP: m_flows_icmp += value; break;
            case PROTOCOL_TCP:  m_flows_tcp += value; break;
            case PROTOCOL_UDP:  m_flows_udp += value; break;
            default:            m_flows_other += value; break;
            }
            m_flows += value;

        } else
        */
        if (field_is(it.field, IPFIX::packetDeltaCount)) {
            uint64_t value = get_uint(it.field);
            switch (protocol) {
            case PROTOCOL_ICMP: m_packets_icmp += value; break;
            case PROTOCOL_TCP:  m_packets_tcp += value; break;
            case PROTOCOL_UDP:  m_packets_udp += value; break;
            default:            m_packets_other += value; break;
            }
            m_packets += value;

        } else if (field_is(it.field, IPFIX::octetDeltaCount)) {
            uint64_t value = get_uint(it.field);
            switch (protocol) {
            case PROTOCOL_ICMP: m_bytes_icmp += value; break;
            case PROTOCOL_TCP:  m_bytes_tcp += value; break;
            case PROTOCOL_UDP:  m_bytes_udp += value; break;
            default:            m_bytes_other += value; break;
            }
            m_bytes += value;

        } else if (field_is(it.field, IPFIX::flowStartSeconds) || field_is(it.field, IPFIX::flowStartMilliseconds)
            || field_is(it.field, IPFIX::flowStartMicroseconds) || field_is(it.field, IPFIX::flowStartNanoseconds)) {
            uint64_t millisecs = get_datetime(it.field);
            if (m_first > millisecs) {
                m_first = millisecs;
            }

        } else if (field_is(it.field, IPFIX::flowEndSeconds) || field_is(it.field, IPFIX::flowEndMilliseconds)
            || field_is(it.field, IPFIX::flowEndMicroseconds) || field_is(it.field, IPFIX::flowEndNanoseconds)) {
            uint64_t millisecs = get_datetime(it.field);
            if (m_last < millisecs) {
                m_last = millisecs;
            }
        }
    }
}

void
Stats::print()
{
    printf("Ident: none\n");
    printf("Flows: %" PRIu64 "\n", m_flows);
    printf("Flows_tcp: %" PRIu64 "\n", m_flows_tcp);
    printf("Flows_udp: %" PRIu64 "\n", m_flows_udp);
    printf("Flows_icmp: %" PRIu64 "\n", m_flows_icmp);
    printf("Flows_other: %" PRIu64 "\n", m_flows_other);
    printf("Packets: %" PRIu64 "\n", m_packets);
    printf("Packets_tcp: %" PRIu64 "\n", m_packets_tcp);
    printf("Packets_udp: %" PRIu64 "\n", m_packets_udp);
    printf("Packets_icmp: %" PRIu64 "\n", m_packets_icmp);
    printf("Packets_other: %" PRIu64 "\n", m_packets_other);
    printf("Bytes: %" PRIu64 "\n", m_bytes);
    printf("Bytes_tcp: %" PRIu64 "\n", m_bytes_tcp);
    printf("Bytes_udp: %" PRIu64 "\n", m_bytes_udp);
    printf("Bytes_icmp: %" PRIu64 "\n", m_bytes_icmp);
    printf("Bytes_other: %" PRIu64 "\n", m_bytes_other);
    printf("First: %" PRIu64 "\n", m_first / 1000);
    printf("Last: %" PRIu64 "\n", m_last / 1000);
    printf("msec_first: %" PRIu64 "\n", m_first % 1000);
    printf("msec_last: %" PRIu64 "\n", m_last % 1000);
    printf("Sequence failures: 0\n");
}
