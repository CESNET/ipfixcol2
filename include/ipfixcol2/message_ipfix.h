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

#include <ipfixcol2/template.h>
#include <stddef.h>

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
	IPX_SET_TEMPLATE_DEF,      /**< Template set (defintions)               */
	IPX_SET_TEMPLATE_WD,       /**< Template set (withdrawals)              */
	IPX_SET_OPT_TEMPLATE_DEF,  /**< Options template set (definitions)      */
	IPX_SET_OPT_TEMPLATE_WD    /**< Options template set (withdrawals)      */
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

	/** Pointer to a raw IPFIX set (starts with a header)                    */
	struct ipfix_set_header *raw_set;
	/** Number of (data or template) records in the set                      */
	unsigned int rec_cnt;

	/**
	 * Properties only for Data sets i.e. type == #IPX_SET_DATA.
	 * Otherwise the fields have undefined values!
	 */
	struct data_set_s {
		/** Template. Can be NULL, when the template is missing.             */
		const ipx_template_t *template;

		/**
		 * Order of the first data record of this set in the list of all
		 * data records (among all data sets) of the IPFIX message.
		 */
		unsigned int first_rec_order;
	} data_set;
};

/**
 * \brief IPFIX message type
 */
typedef struct ipx_ipfix_msg ipx_ipfix_msg_t;

/**
 * \brief Destroy a message with a parsed IPFIX packet
 * \param[out] msg Pointer to the message
 */
void
ipx_ipfix_msg_destroy(ipx_ipfix_msg_t *msg);


// TODO:
// get_odid(), is_source_new(), is_source_closed(),
// get_export_time(), get_seq_number()



/**
 * \brief Get a number of IPFIX sets in the message
 * \param[in] msg Pointer to the message
 * \return Count
 */
size_t
ipx_msg_ipfix_get_set_count(const ipx_ipfix_msg_t *msg);

/**
 * \brief Get a pointer to the set (specified by an index) in the packet
 * \param[in] packet Packet message with sets
 * \param[in] idx    Index of the set
 * \return On success returns the pointer. Otherwise (usually the index is
 *   out-of-range) returns NULL.
 */
struct ipx_ipfix_set *
ipx_ipfix_msg_get_set(ipx_ipfix_msg_t *msg, unsigned int idx);


/**
 * \brief Get a number of IPFIX Data records in the message
 * \param[in] msg Pointer to the message
 * \return Count
 */
size_t
ipx_ipfix_msg_data_rec_count(const ipx_ipfix_msg_t *msg);

/**
 * \brief Get a pointer to the data record (specified by an index) in the packet
 * \param[in] packet Packet message with records
 * \param[in] idx    Index of the record
 * \return On success returns the pointer. Otherwise (usually the index is
 *   out-of-range) returns NULL.
 */
struct ipx_ipfix_data_rec *
ipx_ipfix_msg_data_rec_get(ipx_ipfix_msg_t *msg, size_t idx);

/**@}*/
#endif //IPFIXCOL_MSG_IPFIX_H
