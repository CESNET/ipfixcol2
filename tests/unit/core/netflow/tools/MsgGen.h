#ifndef NF9_MSGGEN_H
#define NF9_MSGGEN_H

#include <string>
#include <limits>
#include <memory>
#include <cstdint>
#include <libfds.h>

// Forward declaration
class nf9_drec;
class nf9_trec;
class nf9_set;

class nf9_buffer {
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
    nf9_buffer();
    /** Destructor  */
    ~nf9_buffer() = default;
    /** Copy constructor */
    nf9_buffer(const nf9_buffer &other);

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

/** \brief NetFlow message  */
class nf9_msg : public nf9_buffer {
private:
    /** Number of records in the message (all types) */
    uint16_t _rec_cnt = 0;
public:
    /** Constructor
     * \note
     *   By default, size and version is set to represent valid NetFlow message. Other header
     *   fields are set to zero.
     */
    nf9_msg();
    /** Destructor  */
    ~nf9_msg() = default;

    void
    set_version(uint16_t version);
    void
    set_count(uint16_t count);
    void
    set_odid(uint32_t odid);
    void
    set_seq(uint32_t seq_num);
    void
    set_time_unix(uint32_t secs);
    void
    set_time_uptime(uint32_t msecs);

    /**
     * \brief Add a Set
     *
     * Number of records in the message header is updated to represent correct count.
     * \param[in] set Set to be added
     */
    void
    add_set(const nf9_set &set);
    /**
     * \brief Release memory allocated for the message
     *
     * \note To delete use free()
     * \warning The object cannot be used after release!!!
     * \return Pointer to the memory
     */
    struct ipx_nf9_msg_hdr *
    release();
};

class nf9_set : public nf9_buffer {
    friend void nf9_msg::add_set(const nf9_set &set);

    uint16_t _rec_cnt = 0;
protected:
    /** Get a memory and update size of Set header
     * \note Size of the Set header is updated to correspond with the size of the buffer
     * \return Pointer to the start of reserved memory
     */
    uint8_t *
    mem_reserve(size_t n);
public:
    nf9_set(uint16_t id);
    ~nf9_set() = default;

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
    add_rec(const nf9_drec &rec);
    /** Add an (Options) Template Record and update size of the header */
    void
    add_rec(const nf9_trec &rec);

    /**
     * \brief Release memory allocated for the message
     *
     * \note To delete use free()
     * \warning The object cannot be used after release!!!
     * \return Pointer to the memory
     */
    struct ipx_nf9_set_hdr *
    release();
};

class nf9_trec : public nf9_buffer {
private:
    friend void nf9_set::add_rec(const nf9_trec &rec);
    /** Type of the template */
    enum fds_template_type _type;
    /** Number of all fields */
    uint16_t _field_cnt = 0;
    /** Number of scope fields */
    uint16_t _scope_cnt = 0;
public:
    /** Variable size of a field */
    static const uint16_t SIZE_VAR = 65535;

    nf9_trec(uint16_t id);
    nf9_trec(uint16_t id, uint16_t scope_cnt);
    ~nf9_trec() = default;

    void
    overwrite_id(uint16_t id);
    void
    overwrite_field_cnt(uint16_t cnt); // Normal Template only
    void
    overwrite_scope_len(uint16_t len); // Options Template only
    void
    overwrite_options_len(uint16_t len); // Options Template only

    /**
     * \brief Add a new definition of a template field
     * \param[in] id   Information Element ID
     * \param[in] len Size in bytes
     */
    void
    add_field(uint16_t id, uint16_t len);
};

class nf9_drec : public nf9_buffer {
    friend void nf9_set::add_rec(const nf9_drec &rec);
public:
    nf9_drec() = default;
    ~nf9_drec() = default;

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
     * \brief Add a string
     *
     * \note
     *   If \p size is smaller that \p value, only first \p size bytes will be copied.
     * \note
     *   If \p size is longer that \p value, string will be appended by zeros.
     * \param[in] value String
     * \param[in] size  Size of the string to copy
     */
    void
    append_string(const std::string &value, uint16_t size);
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
     */
    void
    append_octets(const void *data, uint16_t data_len);
};

#endif //NF9_MSGGEN_H
