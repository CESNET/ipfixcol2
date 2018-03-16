/**
 * \file include/ipfixcol2/session.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Flow source identification (header file)
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

#ifndef IPX_SESSION_H
#define IPX_SESSION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ipfixcol2/api.h>
#include <libfds.h>

/**
 * \defgroup ipxSource Transport session identification
 * \ingroup publicAPIs
 * \brief Transport session interface
 *
 * Data types and API functions for identification and management of Transport
 * session identification. The Exporting Process uses the Transport Session to
 * send messages from multiple _independent_ Observation Domains to the
 * Collecting Process. Moreover, in case of SCTP session messages are also send
 * over _independent_ streams.
 *
 * Following structures represents Transport session between Exporting process
 * and Collecting Process. However, proper processing of flows also requires
 * distinguishing Observation Domain IDs and Stream identifications out of
 * scope of these structures.
 *
 * @{
 */

/**
 * \brief Description of network transport session between an Exporter and
 *   an Collector.
 *
 * We recommend clear the structure using memset before the first use.
 */
struct ipx_session_net {
    uint16_t port_src;         /**< Source port                              */
    uint16_t port_dst;         /**< Destination port                         */
    uint8_t l3_proto;          /**< L3 Protocol type (AF_INET6 or AF_INET)   */

    union {
        struct in6_addr ipv6;  /**< IPv6 address (l3_proto == AF_INET6)      */
        struct in_addr  ipv4;  /**< IPv4 address (l3_proto == AF_INET)       */
    } addr_src;                /**< Source IP address                        */

    union {
        struct in6_addr ipv6;  /**< IPv6 address (l3_proto == AF_INET6)      */
        struct in_addr  ipv4;  /**< IPv4 address (l3_proto == AF_INET)       */
    } addr_dst;                /**< Destination IP address                   */
};

/** Description of TCP transport session parameters */
struct ipx_session_tcp {
    /** Network parameters */
    struct ipx_session_net net;
};

/** Description of UDP transport session parameters */
struct ipx_session_udp {
    /** Network parameters */
    struct ipx_session_net net;

    /** (Options) Template lifetime */
    struct {
        /**
         * Templates that are not received again (i.e. refreshed) within
         * the configured lifetime (in seconds) become invalid. Must be > 0.
         */
        uint16_t tmplts;
        /**
         * Options Templates that are not received again (i.e. refreshed) within
         * the configured lifetime (in seconds) become invalid. Must be > 0.
         */
        uint16_t opts_tmplts;
    } lifetime;
};

/** Description of SCTP transport session parameters */
struct ipx_session_sctp {
    /** Network parameters of primary address */
    struct ipx_session_net net;
};

/** Description of FILE transport session parameters */
struct ipx_session_file {
    /** Full path to the file */
    char *file_path;
};

/**
 * \brief Main session structure
 *
 * Unique identification of Transport Session between an Exporting Process and Collecting Process.
 * \warning Aways use construction functions to create this structure!
 */
struct ipx_session {
    /** Session type */
    enum fds_session_type type;
    /**
     * \brief Identification name
     * \note
     *   For TCP, UDP, SCTP this field represents a source IP address. If corresponding domain name
     *   is also available, it can be mentioned in parenthesis. For example:
     *   "192.168.10.10 (meter1.example.com)"
     * \note
     *   For FILE this field represents basename of the file. For example: "file.201801020000"
     */
    char *ident;

    union {
        struct ipx_session_tcp tcp;     /**< TCP session   */
        struct ipx_session_udp udp;     /**< UDP session   */
        struct ipx_session_sctp sctp;   /**< SCTP session  */
        struct ipx_session_file file;   /**< FILE session  */
    };
};

/**
 * \brief Create a new TCP Transport Session structure
 *
 * This function will also define common parameters of the structure (type, hash, ident) and
 * copy network configuration.
 * \param[in] net User defined network configuration
 * \return On success returns a pointer to newly allocated structure.
 * \return On failure (usually a memory allocation error) returns NULL.
 */
IPX_API struct ipx_session *
ipx_session_new_tcp(const struct ipx_session_net *net);

/**
 * \brief Create a new UDP Transport Session structure
 *
 * This function will also define common parameters of the structure (type, hash, ident) and
 * copy network configuration.
 * \param[in] net     User defined network configuration
 * \param[in] lf_data Template lifetime
 * \param[in] lf_opts Options Template lifetime
 * \return On success returns a pointer to newly allocated structure.
 * \return On failure (usually a memory allocation error) returns NULL.
 */
IPX_API struct ipx_session *
ipx_session_new_udp(const struct ipx_session_net *net, uint16_t lf_data, uint16_t lf_opts);

/**
 * \brief Create a new SCTP Transport Session structure
 * \param[in] net User defined network configuration
 * \return On success returns a pointer to newly allocated structure.
 * \return On failure (usually a memory allocation error) returns NULL.
 */
IPX_API struct ipx_session *
ipx_session_new_sctp(const struct ipx_session_net *net);

/**
 * \brief Create a new FILE Transport Session structure
 * \param[in] file_path Path to the file (cannot be empty)
 * \return On success returns a pointer to newly allocated structure.
 * \return On failure (usually a memory allocation error) returns NULL.
 */
IPX_API struct ipx_session *
ipx_session_new_file(const char *file_path);

/**
 * \brief Destroy a Transport Session structure
 * \param session Transport Session structure to destory
 */
IPX_API void
ipx_session_destroy(struct ipx_session *session);

/**@}*/
#ifdef __cplusplus
}
#endif
#endif // IPX_SESSION_H
