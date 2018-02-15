/**
 * \file include/ipfixcol2/message_ipfix.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief IPFIX message (header file)
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

#ifndef IPFIXCOL_MSG_IPFIX_H
#define IPFIXCOL_MSG_IPFIX_H

#include <libfds.h>
#include <stddef.h>
#include <ipfixcol2.h>
#include "source.h"

/**
 * \endinternal
 * \defgroup ipxMsgIPFIX IPFIX message
 * \ingroup ipxInternalCommonMessage
 * \brief Specification of IPFIX message wrapper for the collector pipeline
 *
 * @{
 */

/** Type of IPFIX message sets */
enum IPX_SET_TYPE {
	IPX_SET_DATA,              /**< Data set                                */
	IPX_SET_TEMPLATE,          /**< Template set (definitions)              */
	IPX_SET_OPT_TEMPLATE       /**< Options template set                    */
};

/**
 * \brief Parsed IPFIX (Data/Template/Option Template) Set
 *
 * In a case of Data set and unknown template, the number of records in the set
 * is always set to "0", because without the corresponding template cannot be
 * calculated.
 */
struct ipx_ipfix_set {
	/** Type of the set                                                      */
	enum IPX_SET_TYPE type;

	/**
	 * Pointer to a raw IPFIX set (starts with a header)
	 * To get real length of the set, you can read value from its header.
	 * i.e. \code{.c} uint16_t real_len = ntohs(raw_set->length); \endcode
	 */
	struct ipfix_set_header *raw_set;
	/** Number of (data or template) records in the set                      */
	unsigned int rec_cnt;

	/**
	 * Properties only for Data sets i.e. type == #IPX_SET_DATA.
	 * Otherwise the fields have undefined values!
	 */
	struct data_set_s {
		/** Template. Can be NULL, when the template is missing.             */
		const struct fds_template *template;

		/**
		 * Index of the first data record of this Set in the list of all
		 * data records (among all data sets) of the IPFIX message.
		 * For example to access first function
		 */
		unsigned int first_rec_idx;
	} data_set;
};

/** \brief Datatype for IPFIX message wrapper                                */
typedef struct ipx_ipfix_msg ipx_ipfix_msg_t;

/**
 * \brief Destroy a message wrapper with a parsed IPFIX packet
 * \param[out] msg Pointer to the message
 */
void
ipx_ipfix_msg_destroy(ipx_ipfix_msg_t *msg);

/**
 * \brief Get pointer to the raw message
 *
 * \warning
 * This function allow to directly access and modify the wrapped massage.
 * It is recommended to use the raw packet only read-only, because inappropriate
 * modifications (e.g. removing/adding sets/records/fields) can cause undefined
 * behavior of API functions.
 *
 * \note Size of the message is stored directly in the header (network byte
 *   order) i.e. \code{.c} uint16_t real_len = ntohs(header->length); \endcode
 * \param[in] msg Pointer to the message wrapper
 * \return
 */
struct ipfix_header *
ipx_ipfix_msg_raw(ipx_ipfix_msg_t *msg);

/**)
 * \brief Get the Source Stream of an IPFIX packet
 *
 * This all you to get information about the Source Stream and Source Session
 * that uniquely describes Exporting Process.
 * \param[in] msg Pointer to the message wrapper
 * \return Pointer to the Source Stream
 */
IPX_API const ipx_stream_t *
ipx_ipfix_msg_stream(const ipx_ipfix_msg_t *msg);

/**
 * \brief Get a number of IPFIX Sets in the message
 * \param[in] msg Pointer to the message wrapper
 * \return Count
 */
IPX_API size_t
ipx_msg_ipfix_set_count(const ipx_ipfix_msg_t *msg);

/**
 * \brief Get a pointer to the Set (specified by an index) in a message
 * \param[in] packet Pointer to the message wrapper with the requrired set
 * \param[in] idx    Index of the set (index starts at 0)
 * \return On success returns the pointer. Otherwise (usually the index is
 *   out-of-range) returns NULL.
 */
IPX_API struct ipx_ipfix_set *
ipx_ipfix_msg_set_get(ipx_ipfix_msg_t *msg, unsigned int idx);

/**
 * \brief Get a number of IPFIX Data records in the message
 * \param[in] msg Pointer to the message
 * \return Count
 */
IPX_API size_t
ipx_ipfix_msg_drec_count(const ipx_ipfix_msg_t *msg);

/**
 * \brief Get a pointer to the data record (specified by an index) in the packet
 * \param[in] packet Packet message with records
 * \param[in] idx    Index of the record (index starts at 0)
 * \return On success returns the pointer. Otherwise (usually the index is
 *   out-of-range) returns NULL.
 */
IPX_API struct ipx_drec *
ipx_ipfix_msg_drec_get(ipx_ipfix_msg_t *msg, size_t idx);

/**@}*/
#endif //IPFIXCOL_MSG_IPFIX_H
