/**
 * \file include/ipfixcol2/source.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Flow source identification (header file)
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

#ifndef IPFIXCOL_SOURCE_H
#define IPFIXCOL_SOURCE_H

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ipfixcol2/api.h>

/**
 * \defgroup ipxSource Flow source identification
 * \ingroup publicAPIs
 * \brief Flow source identification interface
 *
 * Data types and API functions for identification and management of flow
 * sources. Each flow source represents an identification of an IPFIX packet
 * stream (with separately counted sequence number) in a Tranport Session.
 *
 * To identify flow source we use 2 different ID numbers (Session ID and
 * Stream ID). We need the Session ID to uniquely identify a Transport Session
 * (TCP/UDP/SCTP/...) between an Exporting Process and a Collecting Process.
 * This ID should be unique at a time in the scope of this application, but can
 * be reused later.
 *
 * The Exporting Process uses the Transport Session to send
 * flows from multiple _independent_ Observation Domains to the Collecting
 * Process. Because some transfer protocols (e.g. SCTP protocol) can send flows
 * in multiple _independent_ streams per one Observation Domain and each stream
 * creates IPFIX packets with independent sequnce number counters, we need
 * Stream ID. The Stream ID uniquely identifies a combination of an Observation
 * Domain ID (ODID) and a Stream Number in the scope of the Source Session
 * to which it belongs.
 * Some transfer protocols (TCP/UDP/...) allows only one stream per ODID, in
 * this case the Stream Number is always "0".
 * For more information, see ipx_session_get_id() and ipx_stream_get_id().
 *
 * For example, a Session ID X represents a TCP connection between the
 * Exporting Process "A.B.C.D:port" and the Collecting Process "E.F.G.H:port".
 * A Stream ID Y represents the ODID: 42 and the Stream Number: 0.
 * Then combination <X, Y> is a unique identification of the flow source.
 *
 * Summary:  | SOURCE SESSION | (1) <---> (1-*) | SOURCE STREAM |
 *
 * @{
 */

/** \brief Session type of a flow source                                     */
enum IPX_SESSION_TYPE {
	IPX_SESSION_TYPE_UDP,        /**< IPFIX over UDP transfer protocol       */
	IPX_SESSION_TYPE_TCP,        /**< IPFIX over TCP transfer protocol       */
	IPX_SESSION_TYPE_SCTP,       /**< IPFIX over SCTP transfer protocol      */
	IPX_SESSION_TYPE_IPFIX_FILE  /**< IPFIX from IPFIX File format           */
};

/**
 * \brief
 * Description of network transport session between an Exporter and
 * an Collector.
 *
 * We recommend to use memset/bzero before the first use of this structure.
 */
struct ipx_session_addrs {
	uint16_t l3_proto;         /**< L3 Protocol type (AF_INET6 or AF_INET)   */
	uint16_t src_port;         /**< Source port                              */
	uint16_t dst_port;         /**< Destination port                         */

	union {
		struct in6_addr ipv6;  /**< IPv6 address (l3_proto == AF_INET6)      */
		struct in_addr  ipv4;  /**< IPv4 address (l3_proto == AF_INET)       */
	} src_addr;                /**< Source IP address                        */

	union {
		struct in6_addr ipv6;  /**< IPv6 address (l3_proto == AF_INET6)      */
		struct in_addr  ipv4;  /**< IPv4 address (l3_proto == AF_INET)       */
	} dst_addr;                /**< Destination IP address                   */
};

/**
 * \brief Template lifetime information for a UDP session
 */
struct ipx_session_tmplt_timeouts {
	/**
	 * Templates that are not received again (i.e. refreshed) within
	 * the configured lifetime (in seconds) become invalid. Must be > 0.
	 */
	uint32_t template_lifetime;
	/**
	 * Options Templates that are not received again (i.e. refreshed) within
	 * the configured lifetime (in seconds) become invalid. Must be > 0.
	 */
	uint32_t opts_template_lifetime;

	/**
	 * Templates become invalid if they are not included in a sequence of
	 * more this this number of IPFIX messages. Use "0" to disable.
	 */
	uint32_t template_lifepacket;
	/**
	 * Options Templates become invalid if they are not included in a sequence
	 * of more this this number of IPFIX messages. Use "0" to disable.
	 */
	uint32_t opts_template_lifepackets;
};

/**
 * \brief Source session
 *
 * Representation of a Transport Session between an Exporting Process and
 * a Collecting Process. One source session consists of one or more source
 * streams see ::ipx_stream_t.
 */
typedef struct ipx_session ipx_session_t;
/**
 * \brief Source stream
 *
 * Representation of a Data Stream in the context of one source session.
 * This stream represent a unique combination of an Observation Domain ID (ODID)
 * and Stream Identification.
 */
typedef struct ipx_stream ipx_stream_t;

/** \brief Numeric identification of a source session                        */
typedef uint64_t ipx_session_id_t;
/** \brief Numeric identification of a source stream                         */
typedef uint64_t ipx_stream_id_t;


/**
 * \brief Create a new Source Session based on a network connection.
 *
 * For SCTP transfer protocol, try to use only primary IP addresses of both
 * peers.
 * \param[in] type   Type of the session
 * \param[in] addrs  Network addresses and ports (source and destination)
 * \return On success returns a pointer to the session. Otherwise (for example,
 *   memory allocation error) returns NULL.
 */
API ipx_session_t *
ipx_session_create_from_net(enum IPX_SESSION_TYPE type,
		const struct ipx_session_addrs *addrs);
/**
 * \brief Create a new Source Session based on a local file
 * \param[in] type  Type of the session (must be #IPX_SESSION_TYPE_IPFIX_FILE)
 * \param[in] name  Unique identification of the file (usually full path)
 * \return On success returns a pointer to the session. Otherwise (for example,
 *   memory allocation error) returns NULL.
 */
API ipx_session_t *
ipx_session_create_from_file(enum IPX_SESSION_TYPE type, const char *name);

/**
 * \brief Destroy a Source Session identification and all
 * \param[in] source Pointer to a flow source
 */
API void
ipx_session_destroy(ipx_session_t *session);


/**
 * \brief Get a Session ID (identification of a Source Session)
 *
 * The ID represents an internal identification of Transport Session used to
 * transport packets from an Exporting Process to a Collecting Process of this
 * collector. This ID is globally unique, but can be reused after the Transport
 * Session is closed.
 * \param[in] session Pointer to the session
 * \return Session ID
 */
API ipx_session_id_t
ipx_session_get_id(const ipx_session_t *session);

/**
 * \brief Get the name of a Source Session
 *
 * The name of the Session is automatically determined from a network interface
 * description or from a filename identification. Usually useful for printing
 * status/debug messages.
 * \param[in] session Pointer to the session
 * \return Name
 */
API const char *
ipx_session_get_name(const ipx_session_t *session);

/**
 * \brief Get the type of a Source Session
 * \param[in] session Pointer to the session
 * \return Type
 */
API enum IPX_SESSION_TYPE
ipx_session_get_type(const ipx_session_t *session);

/**
 * \brief Get the description of the Transport Session of a Source Session
 *
 * For SCTP protocol with enabled mutlihoming, returns only one source and
 * one destination address. For any file type, returns addresses of
 * the localhost and ports 0.
 * \param[in] session Pointer to the session
 * \return Description of source
 */
API const struct ipx_session_addrs *
ipx_session_get_addrs(const ipx_session_t *session);


/**
 * \brief Set UDP templates configuration
 *
 * This can be configured only for UDP Sessions.
 * \param[in] session  Pointer to a session
 * \param[in] config   New templates configuration
 * \return On success returns 0. Otherwise (non-UDP session, memory allocation
 *   error, etc.) returns non-zero value.
 */
int
ipx_session_set_tmplt_cfg(ipx_session_t *session,
	const struct ipx_session_tmplt_timeouts *config);

/**
 * \brief Get UDP templates configuration
 * \param[in] session  Pointer to a session
 * \return If the configuration is not set, returns NULL. Otherwise returns
 *   a pointer to the configuration,
 */
const struct ipx_session_tmplt_timeouts *
ipx_session_get_tmplt_cfg(const ipx_session_t *session);

/**
 * \brief Find a Source Stream in a Source Session
 *
 * Try to find the Stream that is defined by an Observation Domain ID (ODID)
 * and a Stream Number. For non-SCTP protocols, the parameter \p stream is
 * ignored, because these protocols are single-stream.
 * \param[in] session Pointer to the Source Session
 * \param[in] odid    Observation Domain ID of the Stream
 * \param[in] stream  Stream number
 * \return If the Stream is in the Session, returns pointer to it.
 *   Otherwise (not present) returns NULL.
 */
ipx_stream_t *
ipx_session_find_stream(ipx_session_t *session, uint32_t odid, uint16_t stream);

/**
 * \brief Add a new Source Stream to a Source Session
 *
 * Add the Stream that is defined by an Observation Domain ID (ODID)
 * and a Stream Number to the Source Session. If the Stream is already present,
 * just returns a pointer to it. For non-SCTP protocols, the
 * parameter \p stream is ignored, because these protocols are single-stream.
 * \param[in,out] session Pointer to the Source Session
 * \param[in]     odid    Observation Domain ID
 * \param[in]     stream  Stream number
 * \return On success returns pointer to the (new) Source Stream.
 */
ipx_stream_t *
ipx_session_add_stream(ipx_session_t *session, uint32_t odid, uint16_t stream);


/**
 * \brief Get a Stream ID (identification of a Source Stream)
 *
 * Unique identification of a stream in a scope of Session ID. The ID is based
 * on combination of an Observation Domain ID and a Stream Number.
 * \warning
 *   This number IS unique only in a scope of a Session ID to which it belongs.
 *   To get the Session ID, see ipx_session_get_id(). This ID is NOT globally
 *   (i.e. the scope of the running collector) unique.
 * \param[in] stream Pointer to a stream
 * \return Stream ID
 */
API ipx_stream_id_t
ipx_stream_get_id(const ipx_stream_t *stream);

/**
 * \brief Get the Observation Domain ID (ODID) of a Source Stream
 *
 * The ODID identifies an Observation Domain. Because this ID (in the scope of
 * an Session ID) is shared among all streams that share the same templates,
 * it can be used to determine "Template" scope (useful for a template
 * management).
 * \param[in] stream Pointer to the stream
 * \return ODID
 */
API uint32_t
ipx_stream_get_odid(const ipx_stream_t *stream);

/**
 * \brief Get a Stream Number
 *
 * Represents an identification of an independent data stream in a Transport
 * Session. This number is useful only for SCTP Session
 * (i.e. #IPX_SESSION_TYPE_SCTP), because it uniquely identifies each SCTP
 * stream in a SCTP association. For other session types (i.e. TCP, UDP and
 * FILE) is useless as they always have only one stream per ODID.
 * \param[in] stream Pointer to the stream
 * \return Stream Number
 */
API uint16_t
ipx_stream_get_stream_num(const ipx_stream_t *stream);

/**
 * \brief Get the Session to which a Source Stream belongs
 * \param[in] stream Pointer to the stream
 * \return Pointer to the Source Session
 */
API const ipx_session_t *
ipx_stream_get_session(const ipx_stream_t *stream);

/**@}*/
#endif //IPFIXCOL_SOURCE_H
