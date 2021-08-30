/**
 * \file src/plugins/output/forwarder/src/Sender.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Sender class header
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

#include <functional>
#include "Message.h"
#include "common.h"
#include <ipfixcol2.h>

constexpr size_t TMPLTMSG_MAX_LENGTH = 2500; //NOTE: Maybe this should be configurable from the XML?

/// A class to emit the messages to be sent through the connection in the process of forwarding a message
/// Each connection contains one sender per ODID
class Sender {
public:
    /**
     * \brief The constructor
     * \param emit_callback       The callback to be called when the sender emits a message to be sent
     * \param do_withdrawals      Specifies whether template messages should include withdrawals
     * \param tmplts_resend_pkts  Interval in packets after which templates are resend (0 = never)
     * \param tmplts_resend_secs  Interval in seconds after which templates are resend (0 = never)
     */
    Sender(std::function<void(Message &)> emit_callback, bool do_withdrawals,
           unsigned int tmplts_resend_pkts, unsigned int tmplts_resend_secs);

    /**
     * \brief Receive an IPFIX message and emit messages to be sent to the receiving host
     * \param msg  The IPFIX message
     */
    void
    process_message(ipx_msg_ipfix_t *msg);

    /**
     * \brief Lose an IPFIX message, i.e. update the internal state as if it has been forwarded
     *        even though it is not being sent
     * \param msg  The IPFIX message
     */
    void
    lose_message(ipx_msg_ipfix_t *msg);

    /**
     * \brief Clear the templates state, i.e. force the templates to resend the next round
     */
    void
    clear_templates();

private:
    std::function<void(Message &)> m_emit_callback;

    bool m_do_withdrawals;

    unsigned int m_tmplts_resend_pkts;

    unsigned int m_tmplts_resend_secs;

    uint32_t m_seq_num = 0;

    const fds_tsnapshot_t *m_tsnap = nullptr;

    unsigned int m_pkts_since_tmplts_sent = 0;

    time_t m_last_tmplts_sent_time = 0;

    Message m_message;

    void
    process_templates(const fds_tsnapshot_t *tsnap, uint32_t next_seq_num);

    void
    emit_message();
};