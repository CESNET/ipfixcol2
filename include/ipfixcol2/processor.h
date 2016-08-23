/**
 * \file ipfixcol2/processor.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief IPFIX packet processor (header file)
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

#ifndef IPFIXCOL_PROCESSOR_H
#define IPFIXCOL_PROCESSOR_H

#include <stdint.h>
#include <ipfixcol2/api.h>
#include <ipfixcol2/source.h>
#include <ipfixcol2/message_garbage.h>
#include <ipfixcol2/message_ipfix.h>

/** The datatype of the general message                                      */
typedef struct ipx_processor ipx_processor_t;

/**
 * \brief Create a new IPFIX packet processor
 * \return On success returns pointer to the
 */
API ipx_processor_t *
ipx_processor_create();

/**
 * \brief Destroy a IPFIX packet processor
 * \param[in] processor Pointer to the processor to destoy
 */
API void
ipx_processor_destroy(ipx_processor_t *processor);

/**
 * \brief Parse an IPFIX packet
 *
 * Parse and check validity of all (Data/Template Options Template) Sets in the
 * packet. As a result of this functions is returned a wrapper structure over
 * the packet with API functions for access to parsed template and data records.
 * Because preprocessor also manages templates and other auxiliary structures,
 * sometimes a garbage message can be created (for example, after a template
 * withdrawal request in the packet). In this case, the garbage message is
 * allocated and user of this function must send it down to an IPFIX pipeline
 * where it will be destroyed in appropriate time. The order of insertation of
 * (ipfix vs gargabe) messages doesn't matter. If no garbage message is created,
 * the \p garbage paramater, will be filled with NULL.
 *
 * \param[in]  proc    Pointer to the packet processor
 * \param[in]  msg     Pointer to a raw IPFIX message
 * \param[in]  size    Size of the IPFIX message
 * \param[out] garbage Pointer to the garbage message (must NOT be NULL)
 * \return On success returns a pointer to the wrapper structure with the parsed
 *   packet and possibly generated garbage message. Otherwise (usually malformed
 *   packet or memory allocation error) returns NULL, but the garbage message
 *   can be still created.
 */
API ipx_ipfix_msg_t *
ipx_processor_parse_ipfix(ipx_processor_t *proc, uint8_t *msg,
	uint16_t size, ipx_garbage_msg_t **garbage);

/**
 * \brief Remove internal structurs binded to a specific Source Session
 *
 * This function is useful for cleanup after after source disconnection.
 * Informations such as templates, etc. are removed and inserted into a newly
 * created garbage message.
 *
 * \param[in] proc    Packet processor
 * \param[in] session Source Session ID
 * \return Returns pointer to the garbage message. If the is no garbage binded
 *   to the session, returns NULL.
 */
API ipx_garbage_msg_t *
ipx_processor_remove_session(ipx_processor_t *proc, ipx_session_id_t session);

#endif //IPFIXCOL_PROCESSOR_H
