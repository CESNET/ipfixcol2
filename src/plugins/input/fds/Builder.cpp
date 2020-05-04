/**
 * \file src/plugins/input/fds/Builder.cpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief IPFIX Message builder (implementation)
 * \date May 2020
 */

#include <arpa/inet.h>
#include <cstdlib>

#include "Builder.hpp"
#include "Exception.hpp"


Builder::Builder(uint16_t size)
{
    struct fds_ipfix_msg_hdr *hdr_ptr;

    if (size < FDS_IPFIX_MSG_HDR_LEN) {
        throw FDS_exception("[internal] Invalid size of a message to generate!");
    }

    m_msg.reset((uint8_t *) malloc(size));
    if (!m_msg) {
        throw FDS_exception("Memory allocation error " + std::string(__PRETTY_FUNCTION__));
    }

    // Fill the message header (size will be filled on release)
    hdr_ptr = reinterpret_cast<fds_ipfix_msg_hdr *>(m_msg.get());
    hdr_ptr->version = htons(FDS_IPFIX_VERSION);
    hdr_ptr->odid = 0;
    hdr_ptr->seq_num = 0;
    hdr_ptr->export_time = 0;

    // Positions
    m_msg_alloc = size;
    m_msg_valid = FDS_IPFIX_MSG_HDR_LEN;
    m_set_offset = 0;
    m_set_id = 0; // invalid
}

void
Builder::resize(uint16_t size)
{
    uint8_t *new_ptr = (uint8_t *) realloc(m_msg.get(), size);
    if (!new_ptr) {
        throw FDS_exception("Memory allocation error " + std::string(__PRETTY_FUNCTION__));
    }

    m_msg.release(); // To avoid calling free()
    m_msg.reset(new_ptr);
    m_msg_alloc = size;

    if (m_msg_valid > m_msg_alloc) {
        // The message has been trimmed!
        m_msg_valid = m_msg_alloc;
    }

    if (m_set_offset + FDS_IPFIX_SET_HDR_LEN > m_msg_alloc) {
        // The current offset is out of range
        m_set_offset = 0;
        m_set_id = 0;
    }
}

bool
Builder::empty()
{
    return (!m_msg || m_msg_valid == FDS_IPFIX_MSG_HDR_LEN);
}

uint8_t *
Builder::release()
{
    // Close the current set (if any)
    fset_close();

    // Update IPFIX Message header (size)
    struct fds_ipfix_msg_hdr *hdr_ptr;
    hdr_ptr = reinterpret_cast<fds_ipfix_msg_hdr *>(m_msg.get());
    hdr_ptr->length = htons(m_msg_valid);

    m_msg_alloc = 0;
    m_msg_valid = 0;
    return m_msg.release();
}

/**
 * @brief Create a new Set
 *
 * The previous Set is always closed even if the ID is the same.
 * @param[in] sid Set ID
 * @throw FDS_exception if the Message is full and the Set cannot be created
 */
void
Builder::fset_new(uint16_t sid)
{
    // Close the previous set (if any)
    fset_close();

    // Initialize a new IPFIX Set
    if (FDS_IPFIX_SET_HDR_LEN > m_msg_alloc - m_msg_valid) {
        throw FDS_exception("[internal] Insufficient space for Set in an IPFIX Message");
    }

    m_set_offset = m_msg_valid;
    auto *set_ptr = reinterpret_cast<struct fds_ipfix_set_hdr *>(&m_msg.get()[m_set_offset]);
    set_ptr->flowset_id = htons(sid);
    m_msg_valid += FDS_IPFIX_SET_HDR_LEN;
    m_set_size = FDS_IPFIX_SET_HDR_LEN;
    m_set_id = sid;
}

/**
 * @brief Close the current Set (if any)
 */
void
Builder::fset_close()
{
    if (m_set_offset == 0) {
        return;
    }

    auto *set_ptr = reinterpret_cast<struct fds_ipfix_set_hdr *>(&m_msg.get()[m_set_offset]);
    set_ptr->length = htons(m_set_size);
    m_set_offset = 0;
    m_set_id = 0;
}

void
Builder::set_etime(uint32_t time)
{
    if (!m_msg) {
        throw FDS_exception("[internal] IPFIX Message is not allocated!");
    }

    // Update IPFIX Message header (export time)
    struct fds_ipfix_msg_hdr *hdr_ptr;
    hdr_ptr = reinterpret_cast<fds_ipfix_msg_hdr *>(m_msg.get());
    hdr_ptr->export_time = htonl(time);
}

void
Builder::set_odid(uint32_t odid)
{
    if (!m_msg) {
        throw FDS_exception("[internal] IPFIX Message is not allocated!");
    }

    // Update IPFIX Message header (export time)
    struct fds_ipfix_msg_hdr *hdr_ptr;
    hdr_ptr = reinterpret_cast<fds_ipfix_msg_hdr *>(m_msg.get());
    hdr_ptr->odid = htonl(odid);
}

void
Builder::set_seqnum(uint32_t seq_num)
{
    if (!m_msg) {
        throw FDS_exception("[internal] IPFIX Message is not allocated!");
    }

    // Update IPFIX Message header (export time)
    struct fds_ipfix_msg_hdr *hdr_ptr;
    hdr_ptr = reinterpret_cast<fds_ipfix_msg_hdr *>(m_msg.get());
    hdr_ptr->seq_num = htonl(seq_num);
}

bool
Builder::add_template(const struct fds_template *tmplt)
{
    uint16_t tmplt_len = tmplt->raw.length;
    uint16_t size_req = tmplt_len;
    uint16_t set_id;

    switch (tmplt->type) {
    case FDS_TYPE_TEMPLATE:
        set_id = FDS_IPFIX_SET_TMPLT;
        break;
    case FDS_TYPE_TEMPLATE_OPTS:
        set_id = FDS_IPFIX_SET_OPTS_TMPLT;
        break;
    default:
        throw FDS_exception("[internal] Unexpected Template type cannot be used!");
    }

    if (m_set_offset == 0 || set_id != m_set_id) {
        // New (Options) Template Set must be created
        fset_close();
        size_req += FDS_IPFIX_SET_HDR_LEN;
    }

    if (size_req > m_msg_alloc - m_msg_valid) {
        // Unable to add
        return false;
    }

    if (m_set_offset == 0) {
        fset_new(set_id);
    }

    memcpy(&m_msg.get()[m_msg_valid], tmplt->raw.data, tmplt_len);
    m_msg_valid += tmplt_len;
    m_set_size += tmplt_len;
    return true;
}


bool
Builder::add_record(const struct fds_drec *rec)
{
    uint16_t size_req = rec->size;
    if (m_set_offset == 0 || rec->tmplt->id != m_set_id) {
        // New Data Set must be created
        fset_close();
        size_req += FDS_IPFIX_SET_HDR_LEN;
    }

    if (size_req > m_msg_alloc - m_msg_valid) {
        // Unable to add
        return false;
    }

    if (m_set_offset == 0) {
        fset_new(rec->tmplt->id);
    }

    memcpy(&m_msg.get()[m_msg_valid], rec->data, rec->size);
    m_msg_valid += rec->size;
    m_set_size += rec->size;
    return true;
}

bool
Builder::add_withdrawals()
{
    struct fds_ipfix_trec *rec_ptr = nullptr;
    uint16_t size_req = 2U * FDS_IPFIX_WDRL_ALLSET_LEN;

    if (size_req > m_msg_alloc - m_msg_valid) {
        return false;
    }

    // All Templates Withdrawal
    fset_new(FDS_IPFIX_SET_TMPLT);
    rec_ptr = reinterpret_cast<struct fds_ipfix_trec *>(&m_msg.get()[m_msg_valid]);
    rec_ptr->template_id = htons(FDS_IPFIX_SET_TMPLT);
    rec_ptr->count = htons(0);
    m_msg_valid += 4U; // Only 4 bytes as specified in RFC 7011, 8.1
    m_set_size += 4U;
    fset_close();

    // All Options Template Withdrawal
    fset_new(FDS_IPFIX_SET_OPTS_TMPLT);
    rec_ptr = reinterpret_cast<struct fds_ipfix_trec *>(&m_msg.get()[m_msg_valid]);
    rec_ptr->template_id = htons(FDS_IPFIX_SET_OPTS_TMPLT);
    rec_ptr->count = htons(0);
    m_msg_valid += 4U; // Only 4 bytes as specified in RFC 7011, 8.1
    m_set_size += 4U;
    fset_close();

    return true;
}
