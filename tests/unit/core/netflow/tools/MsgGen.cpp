#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <stdexcept>
#include <iomanip>
#include <iostream>

#include <core/netflow2ipfix/netflow_structs.h>


#include "MsgGen.h"

nf9_buffer::nf9_buffer() : data(nullptr, &free)
{
    data.reset(static_cast<uint8_t *>(std::malloc(size_max * sizeof(uint8_t))));
    size_used = 0;
}

nf9_buffer::nf9_buffer(const nf9_buffer &other) : data(nullptr, &free)
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
nf9_buffer::mem_reserve(size_t n)
{
    if (size_max - size() < n) {
        throw std::runtime_error("Buffer size has been exceeded!");
    }

    uint8_t *ret = &data.get()[size_used];
    size_used += n;
    return ret;
}

void
nf9_buffer::dump()
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
nf9_buffer::release()
{
    // Reallocate the buffer to represent real size of the message during valgrind tests
    const size_t data_size = size();
    uint8_t *data_ptr = data.release();
    return reinterpret_cast<uint8_t *>(realloc(data_ptr, data_size));
}

// -----------------------------------------------------------------------------------------------

nf9_msg::nf9_msg()
{
    constexpr size_t hdr_len = sizeof(struct ipx_nf9_msg_hdr);
    static_assert(hdr_len == IPX_NF9_MSG_HDR_LEN, "Invalid size of Message header!");

    // Reserve header space
    uint8_t *mem = mem_reserve(hdr_len);
    std::memset(mem, 0, hdr_len);

    set_version(IPX_NF9_VERSION);
}

void nf9_msg::set_version(uint16_t version)
{
    auto *hdr = reinterpret_cast<ipx_nf9_msg_hdr *>(front());
    hdr->version = htons(version);
}

void
nf9_msg::set_count(uint16_t count)
{
    auto *hdr = reinterpret_cast<ipx_nf9_msg_hdr *>(front());
    hdr->count = htons(count);
}

void
nf9_msg::set_odid(uint32_t odid)
{
    auto *hdr = reinterpret_cast<ipx_nf9_msg_hdr *>(front());
    hdr->source_id = htonl(odid);
}

void
nf9_msg::set_seq(uint32_t seq_num)
{
    auto *hdr = reinterpret_cast<ipx_nf9_msg_hdr *>(front());
    hdr->seq_number = htonl(seq_num);
}

void
nf9_msg::set_time_unix(uint32_t secs)
{
    auto *hdr = reinterpret_cast<ipx_nf9_msg_hdr *>(front());
    hdr->unix_sec = htonl(secs);
}

void
nf9_msg::set_time_uptime(uint32_t msec)
{
    auto *hdr = reinterpret_cast<ipx_nf9_msg_hdr *>(front());
    hdr->sys_uptime = htonl(msec);
}

void
nf9_msg::add_set(const nf9_set &set)
{
    const uint8_t *src = set.front();
    const size_t size = set.size();

    uint8_t *dst = mem_reserve(size);
    std::memcpy(dst, src, size);

    // Update number of records in the message
    _rec_cnt += set._rec_cnt;
    set_count(_rec_cnt);
}

struct ipx_nf9_msg_hdr *
nf9_msg::release()
{
    return reinterpret_cast<ipx_nf9_msg_hdr *>(nf9_buffer::release());
}

// -----------------------------------------------------------------------------------------------

nf9_set::nf9_set(uint16_t id)
{
    constexpr size_t hdr_len = sizeof(ipx_nf9_set_hdr);
    static_assert(hdr_len == IPX_NF9_SET_HDR_LEN, "Invalid size of Set header!");

    // Reserve header space
    mem_reserve(hdr_len);
    overwrite_id(id);
}

uint8_t*
nf9_set::mem_reserve(size_t n)
{
    uint8_t *ret = nf9_buffer::mem_reserve(n);
    overwrite_len(size());
    return ret;
}

void
nf9_set::add_padding(uint16_t size)
{
    uint8_t *mem = mem_reserve(size);
    std::memset(mem, 0, size);
}

void
nf9_set::overwrite_id(uint16_t id)
{
    auto *hdr = reinterpret_cast<ipx_nf9_set_hdr *>(front());
    hdr->flowset_id = htons(id);
}

void
nf9_set::overwrite_len(uint16_t len)
{
    auto *hdr = reinterpret_cast<ipx_nf9_set_hdr *>(front());
    hdr->length = htons(len);
}

void nf9_set::add_rec(const nf9_drec &rec)
{
    const uint8_t *src = rec.front();
    const size_t size = rec.size();

    uint8_t *dst = mem_reserve(size);
    std::memcpy(dst, src, size);
    ++_rec_cnt;
}

void nf9_set::add_rec(const nf9_trec &rec)
{
    const uint8_t *src = rec.front();
    const size_t size = rec.size();

    uint8_t *dst = mem_reserve(size);
    std::memcpy(dst, src, size);
    ++_rec_cnt;
}

struct ipx_nf9_set_hdr *
nf9_set::release()
{
    return reinterpret_cast<ipx_nf9_set_hdr *>(nf9_buffer::release());
}

// -----------------------------------------------------------------------------------------------

nf9_trec::nf9_trec(uint16_t id, uint16_t scope_cnt)
{
    _type = FDS_TYPE_TEMPLATE_OPTS;
    _field_cnt = 0;
    _scope_cnt = scope_cnt;

    size_t hdr_len = 6U;
    mem_reserve(hdr_len);
    overwrite_id(id);
    overwrite_scope_len(scope_cnt * sizeof(ipx_nf9_tmplt_ie));
    overwrite_options_len(0);
}

nf9_trec::nf9_trec(uint16_t id)
{
    _type = FDS_TYPE_TEMPLATE;
    _field_cnt = 0;
    _scope_cnt = 0;

    size_t hdr_len = 4U;
    mem_reserve(hdr_len);
    overwrite_id(id);
    overwrite_field_cnt(0);
}

void
nf9_trec::overwrite_id(uint16_t id)
{
    auto *hdr = reinterpret_cast<ipx_nf9_opts_trec *>(front());
    hdr->template_id = htons(id);
}

void
nf9_trec::overwrite_field_cnt(uint16_t cnt)
{
    if (_type != FDS_TYPE_TEMPLATE) {
        throw std::runtime_error("Field count cannot be changed in Options Templates");
    }

    auto *hdr = reinterpret_cast<ipx_nf9_trec *>(front());
    hdr->count = htons(cnt);
}

void
nf9_trec::overwrite_scope_len(uint16_t len)
{
    if (_type != FDS_TYPE_TEMPLATE_OPTS) {
        throw std::runtime_error("Scope count cannot be changed in non-Options Templates");
    }

    auto *hdr = reinterpret_cast<ipx_nf9_opts_trec *>(front());
    hdr->scope_length = htons(len);
}

void
nf9_trec::overwrite_options_len(uint16_t len)
{
    if (_type != FDS_TYPE_TEMPLATE_OPTS) {
        throw std::runtime_error("Scope count cannot be changed in non-Options Templates");
    }

    auto *hdr = reinterpret_cast<ipx_nf9_opts_trec *>(front());
    hdr->option_length = htons(len);
}

void
nf9_trec::add_field(uint16_t id, uint16_t len)
{
    ++_field_cnt;

    if (_type == FDS_TYPE_TEMPLATE) {
        // Normal Template
        overwrite_field_cnt(_field_cnt);
    } else {
        // Options Template
        if (_field_cnt > _scope_cnt) {
            // The added field is not from the scope
            overwrite_options_len((_field_cnt - _scope_cnt) * sizeof(ipx_nf9_tmplt_ie));
        }
    }

    auto *ret = reinterpret_cast<ipx_nf9_tmplt_ie *>(mem_reserve(sizeof(ipx_nf9_tmplt_ie)));
    ret->id = htons(id);
    ret->length = htons(len);
}

// -----------------------------------------------------------------------------------------------

void
nf9_drec::append_int(int64_t value, uint16_t size)
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
nf9_drec::append_uint(uint64_t value, uint16_t size)
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
nf9_drec::append_float(double value, uint16_t size)
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
nf9_drec::append_string(const std::string &value, uint16_t size)
{
    size_t str_len = value.size();
    const char *str_data = value.c_str();

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
nf9_drec::append_ip(const std::string &value)
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
nf9_drec::append_octets(const void *data, uint16_t data_len)
{
    if (data_len == 0) {
        // Nothing to do
        return;
    }

    uint8_t *mem = mem_reserve(data_len);
    if (fds_set_octet_array(mem, data_len, data) != FDS_OK) {
        throw std::invalid_argument("fds_set_octet_array() failed!");
    }
}
