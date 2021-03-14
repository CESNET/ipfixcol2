/**
 * \file src/plugins/input/fds/Builder.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief IPFIX Message builder
 * \date May 2020
 */

#ifndef FDS_BUILDER_HPP
#define FDS_BUILDER_HPP

#include <cstdlib>
#include <memory>
#include <libfds.h>

/// IPFIX Message builder
class Builder {
private:
    /// Memory of IPFIX Message to generate (can be nullptr)
    std::unique_ptr<uint8_t, decltype(&free)> m_msg = {nullptr, &free};
    /// Allocated size (bytes)
    uint16_t m_msg_alloc;
    /// Filled size (bytes)
    uint16_t m_msg_valid;

    /// Currently edited Flow Set (zero == invalid)
    uint16_t m_set_offset;
    /// Set ID of the current Flow Set
    uint16_t m_set_id;
    /// Size of the current IPFIX Set
    uint16_t m_set_size;

    void
    fset_new(uint16_t sid);
    void
    fset_close();

public:
    /**
     * @brief Create an IPFIX Message generator
     *
     * By default, ODID, Sequence Number, and Export Time are set to zeros.
     * @param[in] size    Maximal size of the message (allocation size)
     */
    Builder(uint16_t size);
    ~Builder() = default;
    Builder(Builder &&other) = default;

    /**
     * @brief Change maximal size of the message
     *
     * If the size is less than the size of the currently built message, the
     * message is trimmed!
     * @param[in] size Maximal size (i.e. allocation size)
     */
    void
    resize(uint16_t size);
    /**
     * @brief Test if the builder contains an IPFIX Message without content
     *
     * @note The builder is also considered as empty after release().
     * @return True/false
     */
    bool
    empty();
    /**
     * @brief Release the generated IPFIX Message
     * @warning After releasing, the class functions MUST NOT be used anymore!
     * @return Pointer to the message (real size is part of the Message)
     */
    uint8_t *
    release();

    /**
     * @brief Set Export Time of the IPFIX Message
     * @param[in] time Export Time
     */
    void
    set_etime(uint32_t time);
    /**
     * @brief Set Observation Domain ID (ODID) of the IPFIX Message
     * @param[in] odid ODID
     */
    void
    set_odid(uint32_t odid);
    /**
     * @brief Set Sequence Number of the IPFIX Message
     * @param[in] seq_num Sequence Number
     */
    void
    set_seqnum(uint32_t seq_num);

    /**
     * @brief Add an (Options) Template Record
     * @param[in] tmplt IPFIX (Options) Template
     * @return True, if the Template has been successfully added
     * @return False, if the Message is already full
     */
    bool
    add_template(const struct fds_template *tmplt);
    /**
     * @brief Add a Data Record
     * @param[in] rec IPFIX Data Record
     * @return True, if the Record has been successfully added
     * @return False, if the Message is already full
     */
    bool
    add_record(const struct fds_drec *rec);
    /**
     * @brief Add an All (Options) Template Withdrawals (only TCP, SCTP, and File sessions)
     * @note
     *   After calling the function, all previous (Options) Templates are considered to
     *   be invalid.
     * @return True, if the Withdrawals has been successfully added
     * @return False, if the Message is already full
     */
    bool
    add_withdrawals();
};

#endif // FDS_BUILDER_HPP