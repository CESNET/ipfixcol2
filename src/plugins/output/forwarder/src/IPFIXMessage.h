/**
 * \file src/plugins/output/forwarder/src/IPFIXMessage.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Simple IPFIX message wrapper
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

#include <ipfixcol2.h>

class IPFIXMessage
{
public:
    IPFIXMessage(ipx_msg_ipfix_t *msg)
    : msg(msg)
    {}
    
    const ipx_session *
    session()
    {
        ipx_msg_ctx *msg_ctx = ipx_msg_ipfix_get_ctx(msg);
        return msg_ctx->session;
    }

    uint8_t *
    data()
    {
        return ipx_msg_ipfix_get_packet(msg);
    }

    fds_ipfix_msg_hdr *
    header()
    {
        return (fds_ipfix_msg_hdr *)data();
    }

    uint16_t 
    length()
    {
        return ntohs(header()->length);
    }

    uint32_t 
    seq_num()
    {
        return ntohl(header()->seq_num);
    }

    int 
    drec_count()
    {
        return ipx_msg_ipfix_get_drec_cnt(msg);
    }

    uint32_t 
    odid()
    {
        return ntohl(header()->odid);
    }

    const fds_tsnapshot_t *
    get_templates_snapshot()
    {
        if (drec_count() == 0) {
            return NULL;
        }
        return ipx_msg_ipfix_get_drec(msg, 0)->rec.snap;
    }

private:
    ipx_msg_ipfix_t *msg;
};