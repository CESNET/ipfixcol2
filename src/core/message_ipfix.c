/**
 * \file src/msg_packet.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Packet message (source file)
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

#include <ipfixcol2/message_ipfix.h>
#include <libfds.h>
#include <ipfixcol2/source.h>
#include "message_internal.h"

/** Default maximum number of IPFIX sets per message */
#define SET_DEFAULT_CNT (16)

/**
 * \brief Structure for a parsed IPFIX packet
 *
 * This dynamically sized structure wraps a parsed IPFIX message from
 * a source and therefore represents the most frequent type of the pipeline
 * message.
 */
struct ipx_ipfix_msg {
	/** Message type ID. This MUST be always the first element!              */
	struct ipx_msg msg_header;

	/** Raw message from a source                                            */
	struct ipfix_header *raw_msg;
	/** Identification of the Source Steam                                   */
	ipx_stream_t        *src_stream;

	/** Parsed IPFIX (Data/Template/Options Template) Sets                   */
	struct sets_s {
		/** Number of the Sets in the message                                */
		size_t cnt;

		/** Array of sets (valid only when #cnt <= #SET_DEFAULT_CNT)         */
		struct ipx_ipfix_set  base[SET_DEFAULT_CNT];
		/** Array of sets (valid only when #cnt > #SET_DEFAULT_CNT)          */
		struct ipx_ipfix_set *extended;
	} sets;

	/** Info about parsed IPFIX Data records                                 */
	struct records_s {
		/** Size of a single record (depends on registred extensions)        */
		size_t size_per_record;
		/** Number of parsed records                                         */
		size_t record_cnt;
		/** Number of preallocated records                                   */
		size_t record_alloc;
		// Map with extension offsets of the records                         //
		//const ipx_ext_map_t *ext_map;
	} records;

	/**
	 * Array of parsed records.
	 * This MUST be the last element in this structure. To access individual
	 * records MUST use function TODO
	 */
	struct fds_drec rec_data[1];
};

// Get a number of IPFIX sets in the message
size_t
ipx_ipfix_msg_set_count(const ipx_ipfix_msg_t *msg)
{
	return msg->sets.cnt;
}

// Get a pointer to the set (specified by an index) in the packet
struct ipx_ipfix_set *
ipx_ipfix_msg_set_get(ipx_ipfix_msg_t *msg, unsigned int idx)
{
	if (msg->sets.cnt <= idx) {
		return NULL;
	}

	if (msg->sets.cnt <= SET_DEFAULT_CNT) {
		return &msg->sets.base[idx];
	} else {
		return &msg->sets.extended[idx];
	}
}

// Get a number of IPFIX Data records in the message
size_t
ipx_ipfix_msg_data_rec_count(const ipx_ipfix_msg_t *msg)
{
	return msg->records.record_cnt;
}

// Get a pointer to the record (specified by an index) in the packet
struct ipx_drec *
ipx_ipfix_msg_data_rec_get(ipx_ipfix_msg_t *msg, size_t idx)
{
	if (msg->records.record_cnt <= idx) {
		return NULL;
	}

	size_t offset = msg->records.size_per_record * idx;
	uint8_t *rec_start = ((uint8_t *) msg->rec_data) + offset;
	return (struct ipx_drec *) rec_start;
}






