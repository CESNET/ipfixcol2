/**
 * \file src/plugins/output/forwarder/src/Message.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Message class header
 * \date 2021
 */

/* Copyright (C) 2021 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#pragma once

#include <vector>
#include <sys/uio.h>
#include <libfds.h>

/// A class for building IPFIX messages 
class Message {
public:
    /**
     * \brief The default constructor
     */
    Message() {}

    /// Do not allow moving or copying as the parts vector holds addresses to the buffer within the instance
    Message(const Message &) = delete;
    Message(Message &&) = delete;

    /**
     * \brief Start new message, any existing message data is cleared
     * \param msg_hdr  The new message header
     */
    void
    start(const fds_ipfix_msg_hdr *msg_hdr);

    /**
     * \brief Add an IPFIX set
     * \param set  Pointer to the set
     * \warning No data is copied, the pointer is stored directly
     */
    void
    add_set(const fds_ipfix_set_hdr *set);

    /**
     * \brief Add a template
     * \param tmplt  The template
     * \note Unlike add_set, the template data is copied and stored in an internal buffer
     */
    void
    add_template(const fds_template *tmplt);

    /**
     * \brief Add a template withdrawal
     * \param tmplt  The template to withdraw
     */
    void
    add_template_withdrawal(const fds_template *tmplt);

    /**
     * \brief Add a template withdrawal all
     */
    void
    add_template_withdrawal_all();

    /**
     * \brief Finalize the message
     * \warning This HAS to be called AFTER everything was added to the message and BEFORE parts() are accessed.
     */
    void
    finalize();

    /**
     * \brief Access the message parts
     * \return The message parts
     */
    std::vector<iovec> &parts() { return m_parts; }

    /**
     * \brief Get the total length of the message
     * \return The length
     */
    uint16_t length() const { return m_length; }

    /**
     * \brief Check if the message is empty or only contains headers but no content
     * \return true or false
     */
    bool empty() const { return length() <= sizeof(fds_ipfix_msg_hdr) + sizeof(fds_ipfix_set_hdr); }

    /**
     * \brief Access the message header
     * \return The message header
     */
    const fds_ipfix_msg_hdr *header() const { return m_msg_hdr; }

private:
    static constexpr uint16_t BUFFER_SIZE = UINT16_MAX;

    std::vector<iovec> m_parts;    

    uint16_t m_length = 0;

    uint8_t m_buffer[BUFFER_SIZE]; //NOTE: Maybe this should be dynamically expanding instead?

    uint16_t m_buffer_pos = 0;

    fds_ipfix_msg_hdr *m_msg_hdr = nullptr;

    fds_ipfix_set_hdr *m_current_set_hdr = nullptr;

    bool m_last_part_from_buffer = false;

    void
    add_part(uint8_t *data, uint16_t length);

    template <typename T>
    T *
    write(const T *item);

    uint8_t *
    write(const uint8_t *data, uint16_t length);

    void
    require_set(uint16_t set_id);

    void
    finalize_set();
};


template <typename T>
T *
Message::write(const T *item)
{
    T *p = (T *) write((const uint8_t *) item, sizeof(T));
    return p;
}

