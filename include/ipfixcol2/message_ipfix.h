/**
 * \file include/ipfixcol2/message_ipfix.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief IPFIX message (header file)
 * \date 2018
 */

/* Copyright (C) 2018 CESNET, z.s.p.o.
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

#ifndef IPX_MESSSAGE_IPFIX_H
#define IPX_MESSSAGE_IPFIX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libfds.h>
#include <stddef.h>

#include <ipfixcol2/api.h>
#include "session.h"
#include "plugins.h"

/**
 * \defgroup ipxMsgIPFIX IPFIX message
 * \ingroup ipxInternalCommonMessage
 * \brief Specification of IPFIX message wrapper for the collector pipeline
 *
 * @{
 */

/** Data type for IPFIX message wrapper                                       */
typedef struct ipx_msg_ipfix ipx_msg_ipfix_t;
/** Unsigned integer able to hold Stream ID                                   */
typedef uint16_t ipx_stream_t;

/** \brief Packet context                                                     */
struct ipx_msg_ctx {
    /** Transport session                      */
    const struct ipx_session *session;
    /** Observation Domain ID                  */
    uint32_t odid;
    /**
     * \brief Stream ID
     * \note
     *   This value is useful only for SCTP sessions to distinguish individual streams.
     *   For other session the value MUST be set to 0.
     */
    ipx_stream_t stream;
};

/**
 * \brief IPFIX (Data/Template/Option Template) Set information
 */
struct ipx_ipfix_set {
    /**
	 * Pointer to a raw IPFIX set (starts with a header)
	 * To get real length of the Set, you can read value from its header.
	 * i.e. \code{.c} uint16_t real_len = ntohs(ptr->length); \endcode
	 */
    struct fds_ipfix_set_hdr *ptr;

    // New parameters could be added here...
};

/**
 * \brief Data record (record + extensions)
 */
struct ipx_ipfix_record {
    /** Data record information                                               */
    struct fds_drec rec;
    /** Start of reserved space for registered extensions (filled by plugins) */
    uint8_t ext[1];
};

/**
 * \brief Create an empty wrapper around IPFIX (or NetFlow) Message
 *
 * Newly create doesn't have filled information about IPFIX Data/Template/Options Template records
 * and IPFIX Sets in the IPFIX Message. This information must be filled separately using IPFIX
 * parser. In case of NetFlow, the parser transforms message to IPFIX.
 *
 * \warning User MUST make sure that \p msg_data represents valid Message header
 * \param[in] plugin_ctx Context of the plugin
 * \param[in] msg_ctx    Message context (info about Transport Session, ODID, etc.)
 * \param[in] msg_data   Pointer to the IPFIX (or NetFlow) Message header
 * \return Pointer or NULL (memory allocation error)
 */
IPX_API ipx_msg_ipfix_t *
ipx_msg_ipfix_create(const ipx_ctx_t *plugin_ctx, const struct ipx_msg_ctx *msg_ctx,
    uint8_t *msg_data);

/**
 * \brief Destroy a message wrapper with a parsed IPFIX packet
 * \param[out] msg Pointer to the message
 */
IPX_API void
ipx_msg_ipfix_destroy(ipx_msg_ipfix_t *msg);

/**
 * \brief Get a pointer to the raw message
 *
 * \warning
 * This function allow to directly access and modify the wrapped massage. It is recommended to
 * use the raw packet only read-only, because inappropriate modifications (e.g. removing/adding
 * sets/records/fields) can cause undefined behavior of API functions.
 *
 * \note Size of the message is stored directly in the header (network byte
 *   order) i.e. \code{.c} uint16_t real_len = ntohs(header->length); \endcode
 * \param[in] msg Message
 * \return Pointer to the IPFIX (or NetFlow) Message header
 */
IPX_API uint8_t *
ipx_msg_ipfix_get_packet(ipx_msg_ipfix_t *msg);

/**
 * \brief Get the message context
 *
 * The context consists of Transport Session, ODID and Stream identification
 * \param[in] msg Message
 * \return Pointer to the context
 */
IPX_API struct ipx_msg_ctx *
ipx_msg_ipfix_get_ctx(ipx_msg_ipfix_t *msg);

/**
 * \brief Get reference to all (Data/Template/Options Template) sets in the message
 * \param msg  Message
 * \param sets Pointer to the array of sets
 * \param size Number of sets in the array
 */
IPX_API void
ipx_msg_ipfix_get_sets(ipx_msg_ipfix_t *msg, struct ipx_ipfix_set **sets, size_t *size);

/**
 * \brief Get a number of parsed IPFIX Data records in the message
 * \note Data records that a preprocessor failed to interpret are not counted!
 * \param[in] msg Message
 * \return Count
 */
IPX_API uint32_t
ipx_msg_ipfix_get_drec_cnt(const ipx_msg_ipfix_t *msg);

/**
 * \brief Get a pointer to the data record (specified by an index) in the packet
 *
 * \note Data records that a preprocessor failed to interpret are not listed!
 * \param[in] msg Message
 * \param[in] idx Index of the record (index starts at 0)
 * \return On success returns the pointer.
 *   Otherwise (usually the index is out-of-range) returns NULL.
 */
IPX_API struct ipx_ipfix_record *
ipx_msg_ipfix_get_drec(ipx_msg_ipfix_t *msg, uint32_t idx);

/**
 * \brief Cast from a source session message to a base message
 * \param[in] msg Pointer to the session message
 * \return Pointer to the base message.
 */
static inline ipx_msg_t *
ipx_msg_ipfix2base(ipx_msg_ipfix_t *msg)
{
    return (ipx_msg_t *) msg;
}

/**@}*/
#ifdef __cplusplus
}
#endif
#endif // IPX_MESSSAGE_IPFIX_H
