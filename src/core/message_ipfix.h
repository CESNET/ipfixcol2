/**
 * \file src/core/message_ipfix.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief IPFIX Message (internal header file)
 * \date 2016-2018
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

#ifndef IPFIXCOL_MESSAGE_IPFIX_INTERNAL_H
#define IPFIXCOL_MESSAGE_IPFIX_INTERNAL_H

/** Default maximum number of IPFIX sets per message */
#include <ipfixcol2.h>
#include <stdlib.h>
#include "message_base.h"

/** Default number of pre-allocated structures for parser IPFIX Sets         */
#define SET_DEF_CNT (32)
/** Default number of pre-allocated structures for parser IPFIX Data Records */
#define REC_DEF_CNT (64)

/**
 * \brief Structure for a parsed IPFIX Message
 *
 * This dynamically sized structure wraps a parsed IPFIX message from
 * a source and therefore represents the most frequent type of the pipeline
 * message.
 */
struct ipx_msg_ipfix {
    /** Message type ID. This MUST be always the first element!              */
    struct ipx_msg msg_header;

    /** Packet context  */
    struct ipx_msg_ctx ctx;
    /** Raw IPFIX packet from a source (in Network Byte Order)               */
    uint8_t *raw_pkt;
    /** Size of raw message                                                  */
    uint16_t raw_size;

    struct {
        /** Array of sets (valid only when #cnt_valid <= SET_DEF_CNT)       */
        struct ipx_ipfix_set  base[SET_DEF_CNT];
        /** Array of sets (valid only when #cnt_valid > SET_DEF_CNT)        */
        struct ipx_ipfix_set *extended;

        /** Number of the Sets in the message                                */
        uint32_t cnt_valid;
        /** Number of allocated sets                                         */
        uint32_t cnt_alloc;
    } sets; /**< Parsed IPFIX (Data/Template/Options Template) Sets          */

    struct {
        /** Size of a single record (depends on registered extensions)       */
        size_t rec_size;
        /** Number of parsed records                                         */
        uint32_t cnt_valid;
        /** Number of allocated records                                      */
        uint32_t cnt_alloc;
    } rec_info; /**< Parsed IPFIX Data records                               */

    /**
     * Array of parsed records.
     * This MUST be the last element in this structure. To access individual
     * records MUST use function ipx_msg_ipfix_get_drec().
     */
    struct ipx_ipfix_record recs[1];
};

/**
 * \brief Size of a base IPFIX record without any extension
 */
#define IPX_MSG_IPFIX_BASE_REC_SIZE \
    (offsetof(struct ipx_ipfix_record, ext))

/**
 * \brief Size of IPFIX Message wrapper structure
 *
 * For structure able to hold up to \p rec_cnt where each record is \p rec_size bytes.
 * \param[in] rec_cnt  Number of pre-allocated Data Records
 * \param[in] rec_size Size of a Data Record
 * \return Size of the structure
 */
size_t
ipx_msg_ipfix_size(uint32_t rec_cnt, size_t rec_size);

/**
 * \brief Add a new IPFIX Set
 *
 * The record is uninitialized and user MUST fill it!
 * \param[in] msg IPFIX Message wrapper
 * \return Pointer to the record or NULL (memory allocation error)
 */
struct ipx_ipfix_set *
ipx_msg_ipfix_add_set_ref(struct ipx_msg_ipfix *msg);

/**
 * \brief Add a new IPFIX Data Record
 *
 * The record is uninitialized and user MUST fill it!
 * \warning The wrapper \p msg_ref can be reallocated and different pointer can be returned!
 * \param[in,out] msg_ref IPFIX Message wrapper
 * \return Pointer to the record or NULL (memory allocation error)
 */
struct ipx_ipfix_record *
ipx_msg_ipfix_add_drec_ref(struct ipx_msg_ipfix **msg_ref);


#endif // IPFIXCOL_MESSAGE_IPFIX_INTERNAL_H
