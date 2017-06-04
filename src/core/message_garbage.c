/**
 * \file src/message_garbage.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Garbage message (source file)
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

#include <stddef.h>
#include <stdlib.h>

#include <ipfixcol2.h>
#include "message_internal.h"

static const char *msg_module = "Core (message)";

/**
 * \internal
 * \addtogroup ipxGarbageMessage
 * @{
 */

/**
 * \brief Structure of a garbage message
 */
struct ipx_garbage_msg {
	/**
	 * Identification of this message. This MUST be always first in this
	 * structure and the "type" MUST be #IPX_MSG_GARBAGE.
	 */
	struct ipx_msg msg_header;

	/** Object to be destroyed     */
	void *object_ptr;
	/** Object destruction function */
	ipx_garbage_msg_cb object_destructor;
};

// Create a garbage message
API ipx_garbage_msg_t *
ipx_garbage_msg_create(void *object, ipx_garbage_msg_cb callback)
{
	if (object == NULL || callback == NULL) {
		return NULL;
	}

	struct ipx_garbage_msg *msg;
	msg = calloc(1, sizeof(*msg));
	if (!msg) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)",
				  __FILE__, __LINE__);
		return NULL;
	}

	if (ipx_msg_header_init(&msg->msg_header, IPX_MSG_GARBAGE) != 0) {
		MSG_ERROR(msg_module, "Unable to initialize a header of a new garbage"
				"message", NULL);
		free(msg);
		return NULL;
	}

	msg->object_ptr = object;
	msg->object_destructor = callback;
	return msg;
}

// Destroy a garbage message
void
ipx_garbage_msg_destroy(ipx_garbage_msg_t *message)
{
	message->object_destructor(message->object_ptr);
	free(message);
}

/**@}*/
