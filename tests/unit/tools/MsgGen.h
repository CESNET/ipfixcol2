//
// Created by lukashutak on 28/03/18.
//

#ifndef IPFIXCOL_MSGGEN_H
#define IPFIXCOL_MSGGEN_H

#include <string>
#include <limits>
#include <memory>
#include <cstdint>
#include <libfds.h>

// Forward declaration
class ipfix_drec;
class ipfix_trec;
class ipfix_set;

class ipfix_buffer {
private:
    using buffer_ptr = std::unique_ptr<uint8_t, decltype(&std::free)>;

    /** Memory buffer */
    static const size_t size_max = std::numeric_limits<uint16_t>::max();
    /** Message buffer */
    buffer_ptr data;
    /** Allocated size */
    size_t size_used;
protected:
    /** Constructor */
    ipfix_buffer();
    /** Destructor  */
    ~ipfix_buffer() = default;
    /** Copy constructor */
    ipfix_buffer(const ipfix_buffer &other);

    /**
     * \brief Get a memory (past-last-byte) for storing N bytes
     *
     * Mark the next N unused bytes as used
     * \param[in] n Bytes
     * \throw runtime_error if the buffer is not large enough
     * \return Pointer to the start of the reserved memory
     */
    virtual uint8_t *
    mem_reserve(size_t n);

    /**
     * \brief Start of the buffer
     * \return Pointer
     */
    uint8_t *
    front() { return data.get(); }

public:
    /**
     * \brief Get size of used memory
     * \return Bytes
     */
    uint16_t
    size() const { return size_used; };
    /** \copydoc front() */
    const uint8_t *
    front() const { return data.get(); }

    /**
     * \brief Print content of the buffer on the standard output
     */
    void
    dump();

    /**
     * \brief Release memory allocated for the message
     *
     * \note To delete use free()
     * \warning The object cannot be used after release!!!
     * \return Pointer to the memory
     */
    uint8_t *
    release();
};

/** \brief IPFIX message  */
class ipfix_msg : public ipfix_buffer {
public:
    /** Constructor
     * \note
     *   By default, size and version is set to represent valid IPFIX message. Other header
     *   fields are set to zero.
     */
    ipfix_msg();
    /** Destructor  */
    ~ipfix_msg() = default;

    void
    set_version(uint16_t version);
    void
    set_len(uint16_t len);
    void
    set_odid(uint32_t odid);
    void
    set_seq(uint32_t seq_num);
    void
    set_exp(uint32_t exp_time);

    /**
     * \brief Add a Set
     *
     * Size of the message header is updated to represent correct size.
     * \param[in] set Set to be added
     */
    void
    add_set(const ipfix_set &set);
    /**
     * \brief Release memory allocated for the message
     *
     * \note To delete use free()
     * \warning The object cannot be used after release!!!
     * \return Pointer to the memory
     */
    struct fds_ipfix_msg_hdr *
    release();
};

class ipfix_set : public ipfix_buffer {
    friend void ipfix_msg::add_set(const ipfix_set &set);
protected:
    /** Get a memory and update size of Set header
     * \note Size of the Set header is updated to correspond with the size of the buffer
     * \return Pointer to the start of reserved memory
     */
    uint8_t *
    mem_reserve(size_t n);
public:
    ipfix_set(uint16_t id);
    ~ipfix_set() = default;

    /**
     * \brief Change Set ID (modify Set header)
     * \param[in] id New ID
     */
    void
    overwrite_id(uint16_t id);
    /**
     * \brief Change length of the Set (modify Set header)
     * \param[in] len New length
     */
    void
    overwrite_len(uint16_t len);
    /**
     * \brief Add padding (zeros) after the last record (increase used size)
     * \param[in] size Number of bytes
     */
    void
    add_padding(uint16_t size);

    /** Add a Data Record and update size of the header */
    void
    add_rec(const ipfix_drec &rec);
    /** Add an (Options) Template Record and update size of the header */
    void
    add_rec(const ipfix_trec &rec);

    /**
     * \brief Release memory allocated for the message
     *
     * \note To delete use free()
     * \warning The object cannot be used after release!!!
     * \return Pointer to the memory
     */
    struct fds_ipfix_set_hdr *
    release();
};

class ipfix_trec : public ipfix_buffer {
private:
    friend void ipfix_set::add_rec(const ipfix_trec &rec);
    /** Type of the template */
    enum fds_template_type _type;
    /** Number of fields */
    uint16_t _field_cnt = 0;
public:
    /** Variable size of a field */
    static const uint16_t SIZE_VAR = 65535;

    ipfix_trec(uint16_t id);
    ipfix_trec(uint16_t id, uint16_t scope_cnt);
    ~ipfix_trec() = default;

    void
    overwrite_id(uint16_t id);
    void
    overwrite_scope_cnt(uint16_t cnt);
    void
    overwrite_field_cnt(uint16_t len);

    /**
     * \brief Add a new definition of a template field
     *
     * If \p en is zero, Enterprise fields is not added.
     * \warning For testing purposes, \p id is not checked for its max. value.
     * \param[in] id   Information Element ID
     * \param[in] len Size in bytes (#SIZE_VAR represents variable-length field)
     * \param[in] en   Enterprise number
     */
    void
    add_field(uint16_t id, uint16_t len, uint32_t en = 0);
};

class ipfix_drec : public ipfix_buffer {
    friend void ipfix_set::add_rec(const ipfix_drec &rec);
public:
    /** Automatically calculate static size */
    static const uint16_t SIZE_AUTO = 0;
    /** Store field as variable-length field (i.e. add header) */
    static const uint16_t SIZE_VAR = 65535;

    ipfix_drec() = default;
    ~ipfix_drec() = default;

    /**
     * \brief Add variable-length field header
     * \param[in] size       Size (in bytes)
     * \param[in] force_long Always use longer version
     */
    void
    var_header(uint16_t size, bool force_long = false);

    /**
     * \brief Add an integer value
     * \param[in] value Value
     * \param[in] size  Range 1..8 bytes
     */
    void
    append_int(int64_t value, uint16_t size = 8);
    /**
     * \brief Add an interger value
     * \param[in] value Value
     * \param[in] size  Range 1..8 bytes
     */
    void
    append_uint(uint64_t value, uint16_t size = 8);
    /**
     * \brief Add a float value
     * \param[in] value Value
     * \param[in] size  4 or 8 bytes
     */
    void
    append_float(double value, uint16_t size = 8);
    /**
     * \brief Add a boolean value
     * \param[in] value True or false
     */
    void
    append_bool(bool value);
    /**
     * \brief Add a string
     *
     * \note
     *   If \p size is #SIZE_VAR, the functions will automatically add variable-length head and
     *   copy whole string.
     * \note
     *   If \p size is smaller that \p value, only first \p size bytes will be copied.
     * \note
     *   If \p size is longer that \p value, string will be appended by zeros.
     * \param[in] value String
     * \param[in] size  Size of the string to copy or #SIZE_VAR
     */
    void
    append_string(const std::string &value, uint16_t size = SIZE_VAR);
    /**
     * \brief Add a timestamp (high precision)
     * \param ts   Timestamp
     * \param type Type of timestamp
     */
    void
    append_datetime(struct timespec ts, enum fds_iemgr_element_type type);
    /**
     * \brief Add a timestamp (low precision)
     * \param ts   Timestamp
     * \param type Type of timestamp
     */
    void
    append_datetime(uint64_t ts, enum fds_iemgr_element_type type);
    /**
     * \brief Add an IPv4/IPv6 address
     * \note Type and size (4 or 16 bytes) is automatically determined.
     * \param value Address to add
     */
    void
    append_ip(const std::string &value);
    /**
     * \brief Add a MAC address
     * \note Size is always 6 bytes
     * \param[in] value Address to add
     */
    void
    append_mac(const std::string &value);
    /**
     * \brief Add an octet array
     *
     * \param[in] data  Array of octets
     * \param[in] size  Size of the array to copy
     * \param[in] var_field Add variable-length header
     */
    void
    append_octets(const void *data, uint16_t data_len, bool var_field = true);
};

#endif //IPFIXCOL_MSGGEN_H
