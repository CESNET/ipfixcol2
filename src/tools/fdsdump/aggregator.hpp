#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <libfds.h>

constexpr std::size_t BUCKETS_COUNT = 4096;

struct ip_address_s
{
    uint8_t length;
    uint8_t address[16];
};

struct aggregate_record_s
{
    struct __attribute__((packed)) key_s
    {
        uint8_t protocol;
        ip_address_s src_ip;
        uint16_t src_port;
        ip_address_s dst_ip;
        uint16_t dst_port;
    } key;

    uint64_t packets;
    uint64_t bytes;
    uint64_t flows;

    aggregate_record_s *next;
};

class Aggregator
{
public:
    Aggregator();

    void
    process_record(fds_drec &drec);

    std::vector<aggregate_record_s *>
    records()
    {
        return m_records;
    }

private:
    std::array<aggregate_record_s *, BUCKETS_COUNT> m_buckets;
    std::vector<aggregate_record_s *> m_records;
};