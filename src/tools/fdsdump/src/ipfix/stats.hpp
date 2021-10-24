#pragma once

#include <libfds.h>
#include <ipfixcol2.h>

class Stats {
public:
    void
    process_record(fds_drec &drec);

    void
    print();

private:
    uint64_t m_flows = 0;
    uint64_t m_flows_tcp = 0;
    uint64_t m_flows_udp = 0;
    uint64_t m_flows_icmp = 0;
    uint64_t m_flows_other = 0;
    uint64_t m_packets = 0;
    uint64_t m_packets_tcp = 0;
    uint64_t m_packets_udp = 0;
    uint64_t m_packets_icmp = 0;
    uint64_t m_packets_other = 0;
    uint64_t m_bytes = 0;
    uint64_t m_bytes_tcp = 0;
    uint64_t m_bytes_udp = 0;
    uint64_t m_bytes_icmp = 0;
    uint64_t m_bytes_other = 0;
    uint64_t m_first = UINT64_MAX;
    uint64_t m_last = 0;

};