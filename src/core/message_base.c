/**
 * \file src/core/message_base.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Common specification of messages for the IPFIXcol pipeline (source file)
 * \date 2016-2018
 */

/* Copyright (C) 2016-2018 CESNET, z.s.p.o.
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

#include <ipfixcol2.h>
#include "message_base.h"
#include "message_terminate.h"

// Get the type of a message for the collector pipeline
enum ipx_msg_type
ipx_msg_get_type(ipx_msg_t *msg)
{
    return msg->type;
}

// Destroy a message for the collector pipeline
void
ipx_msg_destroy(ipx_msg_t *msg)
{
    switch (msg->type) {
    case IPX_MSG_IPFIX:
        ipx_msg_ipfix_destroy(ipx_msg_base2ipfix(msg));
        break;
    case IPX_MSG_SESSION:
        ipx_msg_session_destroy(ipx_msg_base2session(msg));
        break;
    case IPX_MSG_GARBAGE:
        ipx_msg_garbage_destroy(ipx_msg_base2garbage(msg));
        break;
    case IPX_MSG_TERMINATE:
        ipx_msg_terminate_destroy(ipx_msg_base2terminate(msg));
        break;
    }
}
