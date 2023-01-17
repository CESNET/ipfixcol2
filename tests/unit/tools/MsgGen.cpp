//
// Created by lukashutak on 28/03/18.
//

#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <stdexcept>
#include <libfds/ipfix_structs.h>
#include <iomanip>
#include <iostream>


#include "MsgGen.h"

ipfix_buffer::ipfix_buffer() : data(nullptr, &free)
{
    data.reset(static_cast<uint8_t *>(std::malloc(size_max * sizeof(uint8_t))));
    size_used = 0;
}

ipfix_buffer::ipfix_buffer(const ipfix_buffer &other) : data(nullptr, &free)
{
    uint8_t *data2copy = static_cast<uint8_t *>(std::malloc(size_max * sizeof(uint8_t)));
    if (!data2copy) {
        throw std::runtime_error("malloc() failed!");
    }

    std::memcpy(data2copy, other.data.get(), other.size_used);
    data.reset(data2copy);
    size_used = 0;
}

uint8_t*
ipfix_buffer::mem_reserve(size_t n)
{
    if (size_max - size() < n) {
        throw std::runtime_error("Buffer size has been exceeded!");
    }

    uint8_t *ret = &data.get()[size_used];
    size_used += n;
    return ret;
}

void
ipfix_buffer::dump()
{
    // Setup
    std::ios::fmtflags flags(std::cout.flags());
    std::cout << std::hex << std::setfill('0');

    // Print
    for (size_t i = 0; i < size(); ++i) {
        std::cout << std::setw(2) <<int(data.get()[i]) << " ";

        if ((i + 1) % 16 == 0) {
            std::cout << std::endl;
        }
    }

    std::cout << std::endl;

    // Cleanup
    std::cout.flags(flags);
}

uint8_t *
ipfix_buffer::release()
{
    // Reallocate the buffer to represent real size of the message during valgrind tests
    const size_t data_size = size();
    uint8_t *data_ptr = data.release();
    return reinterpret_cast<uint8_t *>(realloc(data_ptr, data_size));
}

// -----------------------------------------------------------------------------------------------

ipfix_msg::ipfix_msg()
{
    constexpr size_t hdr_len = sizeof(fds_ipfix_msg_hdr);
    static_assert(hdr_len == FDS_IPFIX_MSG_HDR_LEN, "Invalid size of Message header!");

    // Reserve header space
    uint8_t *mem = mem_reserve(hdr_len);
    std::memset(mem, 0, hdr_len);

    set_version(FDS_IPFIX_VERSION);
    set_len(hdr_len);
}

void ipfix_msg::set_version(uint16_t version)
{
    auto *hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(front());
    hdr->version = htons(version);
}

void
ipfix_msg::set_len(uint16_t len)
{
    auto *hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(front());
    hdr->length = htons(len);
}

void
ipfix_msg::set_odid(uint32_t odid)
{
    auto *hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(front());
    hdr->odid = htonl(odid);
}

void
ipfix_msg::set_seq(uint32_t seq_num)
{
    auto *hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(front());
    hdr->seq_num = htonl(seq_num);
}

void
ipfix_msg::set_exp(uint32_t exp_time)
{
    auto *hdr = reinterpret_cast<fds_ipfix_msg_hdr *>(front());
    hdr->export_time = htonl(exp_time);
}

void
ipfix_msg::add_set(const ipfix_set &set)
{
    const uint8_t *src = set.front();
    const size_t size = set.size();

    uint8_t *dst = mem_reserve(size);
    std::memcpy(dst, src, size);

    set_len(ipfix_msg::size());
}

struct fds_ipfix_msg_hdr *
ipfix_msg::release()
{
    return reinterpret_cast<fds_ipfix_msg_hdr *>(ipfix_buffer::release());
}

// -----------------------------------------------------------------------------------------------

ipfix_set::ipfix_set(uint16_t id)
{
    constexpr size_t hdr_len = sizeof(fds_ipfix_set_hdr);
    static_assert(hdr_len == FDS_IPFIX_SET_HDR_LEN, "Invalid size of Set header!");

    // Reserve header space
    mem_reserve(hdr_len);
    overwrite_id(id);
}

uint8_t*
ipfix_set::mem_reserve(size_t n)
{
    uint8_t *ret = ipfix_buffer::mem_reserve(n);
    overwrite_len(size());
    return ret;
}

void
ipfix_set::add_padding(uint16_t size)
{
    uint8_t *mem = mem_reserve(size);
    std::memset(mem, 0, size);
}

void
ipfix_set::overwrite_id(uint16_t id)
{
    auto *hdr = reinterpret_cast<fds_ipfix_set_hdr *>(front());
    hdr->flowset_id = htons(id);
}

void
ipfix_set::overwrite_len(uint16_t len)
{
    auto *hdr = reinterpret_cast<fds_ipfix_set_hdr *>(front());
    hdr->length = htons(len);
}

void ipfix_set::add_rec(const ipfix_drec &rec)
{
    const uint8_t *src = rec.front();
    const size_t size = rec.size();

    uint8_t *dst = mem_reserve(size);
    std::memcpy(dst, src, size);
}

void ipfix_set::add_rec(const ipfix_trec &rec)
{
    const uint8_t *src = rec.front();
    const size_t size = rec.size();

    uint8_t *dst = mem_reserve(size);
    std::memcpy(dst, src, size);
}

struct fds_ipfix_set_hdr *
ipfix_set::release()
{
    return reinterpret_cast<fds_ipfix_set_hdr *>(ipfix_buffer::release());
}

// -----------------------------------------------------------------------------------------------

ipfix_trec::ipfix_trec(uint16_t id, uint16_t scope_cnt)
{
    _type = FDS_TYPE_TEMPLATE_OPTS;
    _field_cnt = 0;

    size_t hdr_len = 6U;
    mem_reserve(hdr_len);
    overwrite_id(id);
    overwrite_scope_cnt(scope_cnt);
    overwrite_field_cnt(0);
}

ipfix_trec::ipfix_trec(uint16_t id)
{
    _type = FDS_TYPE_TEMPLATE;
    _field_cnt = 0;

    size_t hdr_len = 4U;
    mem_reserve(hdr_len);
    overwrite_id(id);
    overwrite_field_cnt(0);
}

void
ipfix_trec::overwrite_id(uint16_t id)
{
    auto *hdr = reinterpret_cast<fds_ipfix_opts_trec *>(front());
    hdr->template_id = htons(id);
}

void
ipfix_trec::overwrite_scope_cnt(uint16_t cnt)
{
    if (_type != FDS_TYPE_TEMPLATE_OPTS) {
        throw std::runtime_error("Scope count cannot be changed in non-Options Templates");
    }

    auto *hdr = reinterpret_cast<fds_ipfix_opts_trec *>(front());
    hdr->scope_field_count = htons(cnt);
}

void
ipfix_trec::overwrite_field_cnt(uint16_t len)
{
    auto *hdr = reinterpret_cast<fds_ipfix_opts_trec *>(front());
    hdr->count = htons(len);
}

void
ipfix_trec::add_field(uint16_t id, uint16_t len, uint32_t en)
{
    size_t rec_size;
    ++_field_cnt;
    overwrite_field_cnt(_field_cnt);

    if (en != 0) {
        rec_size = 8U;
        id |= 0x8000;
    } else {
        rec_size = 4U;
    }

    auto *ret = reinterpret_cast<fds_ipfix_tmplt_ie *>(mem_reserve(rec_size));
    ret[0].ie.id = htons(id);
    ret[0].ie.length = htons(len);

    if (en == 0) {
        return;
    }

    ret[1].enterprise_number = htonl(en);
}

// -----------------------------------------------------------------------------------------------

void
ipfix_drec::var_header(uint16_t size, bool force_long)
{
    uint8_t *mem;

    if (force_long || size >= std::numeric_limits<uint8_t>::max()) {
        mem = mem_reserve(3U);
        mem[0] = std::numeric_limits<uint8_t>::max();
        uint16_t *full_len = reinterpret_cast<uint16_t *>(&mem[1]);
        full_len[0] = htons(size);
    } else {
        mem = mem_reserve(1U);
        mem[0] = uint8_t(size);
    }
}

void
ipfix_drec::append_int(int64_t value, uint16_t size)
{
    if (size < 1U || size > 8U) {
        throw std::invalid_argument("Invalid field size");
    }

    uint8_t *mem = mem_reserve(size);
    if (fds_set_int_be(mem, size, value) != FDS_OK) {
        throw std::invalid_argument("fds_set_int_be() failed!");
    }
}

void
ipfix_drec::append_uint(uint64_t value, uint16_t size)
{
    if (size < 1U || size > 8U) {
        throw std::invalid_argument("Invalid field size");
    }

    uint8_t *mem = mem_reserve(size);
    if (fds_set_uint_be(mem, size, value) != FDS_OK) {
        throw std::invalid_argument("fds_set_uint_be() failed!");
    }
}

void
ipfix_drec::append_float(double value, uint16_t size)
{
    if (size != 4 && size != 8) {
        throw std::invalid_argument("Invalid field size");
    }

    uint8_t *mem = mem_reserve(size);
    if (fds_set_float_be(mem, size, value) != FDS_OK) {
        throw std::invalid_argument("fds_set_float_be() failed!");
    }
}

void
ipfix_drec::append_bool(bool value)
{
    uint8_t *mem = mem_reserve(1U);
    if (fds_set_bool(mem, 1U, value) != FDS_OK) {
        throw std::invalid_argument("fds_set_bool() failed!");
    }
}

void
ipfix_drec::append_string(const std::string &value, uint16_t size)
{
    size_t str_len = value.size();
    const char *str_data = value.c_str();

    if (size == SIZE_AUTO) {
        throw std::invalid_argument("SIZE_AUTO is not supported!");
    }

    if (size == SIZE_VAR) {
        var_header(str_len);
        size = str_len;
    }

    uint8_t *mem = mem_reserve(size);
    size_t mem_cpy = (str_len < size) ? str_len : size;

    if (fds_set_string(mem, mem_cpy, str_data) != FDS_OK) {
        throw std::invalid_argument("fds_set_string() failed!");
    }

    if (str_len < size) {
        // Fill the rest with zeros
        std::memset(&mem[mem_cpy], 0, size - str_len);
    }
}

void
ipfix_drec::append_datetime(struct timespec ts, enum fds_iemgr_element_type type)
{
    size_t size;
    switch (type) {
    case FDS_ET_DATE_TIME_SECONDS:      size = 4U; break;
    case FDS_ET_DATE_TIME_MILLISECONDS: size = 8U; break;
    case FDS_ET_DATE_TIME_MICROSECONDS: size = 8U; break;
    case FDS_ET_DATE_TIME_NANOSECONDS:  size = 8U; break;
    default:
        throw std::invalid_argument("Invalid type of timestamp!");
    }

    uint8_t *mem = mem_reserve(size);
    if (fds_set_datetime_hp_be(mem, size, type, ts) != FDS_OK) {
        throw std::invalid_argument("fds_set_datetime_hp_be() failed!");
    }
}

void
ipfix_drec::append_datetime(uint64_t ts, enum fds_iemgr_element_type type)
{
    size_t size;
    switch (type) {
    case FDS_ET_DATE_TIME_SECONDS:      size = 4U; break;
    case FDS_ET_DATE_TIME_MILLISECONDS: size = 8U; break;
    case FDS_ET_DATE_TIME_MICROSECONDS: size = 8U; break;
    case FDS_ET_DATE_TIME_NANOSECONDS:  size = 8U; break;
    default:
        throw std::invalid_argument("Invalid type of timestamp!");
    }

    uint8_t *mem = mem_reserve(size);
    if (fds_set_datetime_lp_be(mem, size, type, ts) != FDS_OK) {
        throw std::invalid_argument("fds_set_datetime_lp_be() failed!");
    }
}

void
ipfix_drec::append_ip(const std::string &value)
{
    uint8_t buffer[sizeof(struct in6_addr)];
    size_t size;

    if (inet_pton(AF_INET, value.c_str(), buffer) == 1) {
        size = sizeof(struct in_addr);
    } else if (inet_pton(AF_INET6, value.c_str(), buffer) == 1) {
        size = sizeof(struct in6_addr);
    } else {
        throw std::invalid_argument("Unable to parse IPv4/IPv6 address!");
    }

    uint8_t *mem = mem_reserve(size);
    if (fds_set_ip(mem, size, buffer) != FDS_OK) {
        throw std::invalid_argument("fds_set_ip() failed!");
    }
}

void
ipfix_drec::append_octets(const void *data, uint16_t data_len, bool var_field)
{
    if (var_field) {
        var_header(data_len);
    }

    uint8_t *mem = mem_reserve(data_len);
    if (fds_set_octet_array(mem, data_len, data) != FDS_OK) {
        throw std::invalid_argument("fds_set_octet_array() failed!");
    }
}
