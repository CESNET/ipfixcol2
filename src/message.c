/**
 * \file src/message.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Common specification of messages for the IPFIXcol pipeline
 *   (source file)
 * \date 2016
 */

/* Copyright (C) 2016 CESNET, z.s.p.o.
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
#include <ipfixcol2/message_garbage.h>
#include "message_internal.h"

// Get the type of a message for the collector pipeline
enum IPX_MSG_TYPE
ipx_msg_get_type(ipx_msg_t *message)
{
	return message->type;
}

// Destroy a message for the collector pipeline
void
ipx_msg_destroy(ipx_msg_t *message)
{
	switch (message->type) {
	case IPX_MSG_IPFIX:
		// TODO
		break;
	case IPX_MSG_CONN_STATUS:
		// TODO
		break;
	case IPX_MSG_GARBAGE:
		// Destroy a garbage message
		ipx_garbage_msg_destroy((ipx_garbage_msg_t *) message);
		break;
	}
}
