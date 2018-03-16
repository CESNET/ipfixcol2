/**
 * \file src/core/session.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Flow source identification (source file)
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

// Get GNU specific basename() function
#define _GNU_SOURCE
#include <string.h>

#include <stdint.h>
#include <ipfixcol2.h>
#include <stdlib.h>
#include <inttypes.h>

/**
 * \brief Create a source description string from a Network Session structure
 *
 * Output format: "<src_ip>:<src_port> (hostname)". If the hostname cannot be determined,
 * the parenthesis will be omitted.
 * \param[in] net Network Session structure
 * \return On success returns a newly allocated string (MUST be freed by user). Otherwise returns
 *   NULL.
 */
static char *
session_net2str(const struct ipx_session_net *net)
{
    const size_t addr_len = INET6_ADDRSTRLEN; // Max. length of IPv6 address
    const size_t port_len = 5U; // Max. length of port ((2^16)-1)
    const size_t aux_len = 2U; // 1x ":" + 1x '\0'
    const size_t total_len = addr_len + port_len + aux_len;

    char *buffer = malloc(total_len * sizeof(char));
    if (!buffer) {
        return NULL;
    }

    // Source IPv4/IPv6 address
    if (inet_ntop(net->l3_proto, &net->addr_src, buffer, total_len) == NULL) {
        // Something bad happened
        free(buffer);
        return NULL;
    }

    // Source port
    size_t pos = strlen(buffer);
    buffer[pos++] = ':';
    sprintf(&buffer[pos], "%" PRIu16, net->port_src);
    return buffer;
}

/**
 * \brief Common constructor for Network based sessions
 * \param[in] net  Network session structure
 * \param[in] type Type of session (only TCP, UDP and SCTP)
 * \return Pointer to the session or NULL (memory allocation error).
 */
static inline struct ipx_session *
session_net_common(const struct ipx_session_net *net, enum fds_session_type type)
{
    struct ipx_session *res = calloc(1, sizeof(*res));
    if (!res) {
        return NULL;
    }

    switch (type) {
    case FDS_SESSION_TCP:
        res->tcp.net = *net;
        break;
    case FDS_SESSION_UDP:
        res->udp.net = *net;
        break;
    case FDS_SESSION_SCTP:
        res->sctp.net = *net;
        break;
    default:
        free(res);
        return NULL;
    }

    res->type = type;
    res->ident = session_net2str(net);
    if (!res->ident) {
        free(res);
        return NULL;
    }

    return res;
}

struct ipx_session *
ipx_session_new_tcp(const struct ipx_session_net *net)
{
    return session_net_common(net, FDS_SESSION_TCP);
}

struct ipx_session *
ipx_session_new_udp(const struct ipx_session_net *net, uint16_t lf_data, uint16_t lf_opts)
{
    struct ipx_session *res = session_net_common(net, FDS_SESSION_UDP);
    if (res != NULL) {
        res->udp.lifetime.tmplts = lf_data;
        res->udp.lifetime.opts_tmplts = lf_opts;
    }

    return res;
}

struct ipx_session *
ipx_session_new_sctp(const struct ipx_session_net *net)
{
    return session_net_common(net, FDS_SESSION_SCTP);
}

struct ipx_session *
ipx_session_new_file(const char *file_path)
{
    if (strlen(file_path) == 0) {
        return NULL;
    }

    struct ipx_session *res = calloc(1, sizeof(*res));
    if (!res) {
        return NULL;
    }

    res->type = FDS_SESSION_FILE;
    res->file.file_path = strdup(file_path);
    if (!res->file.file_path) {
        free(res);
        return NULL;
    }

    res->ident = basename(res->file.file_path); // GNU specific basename()
    if (!res->ident) {
        free(res);
        return NULL;
    }

    if (strlen(res->ident) == 0) {
        // Unable to determined basename from the path -> use the full path
        res->ident = res->file.file_path;
    }

    return res;
}

void
ipx_session_destroy(struct ipx_session *session)
{
    if (session->type == FDS_SESSION_FILE) {
        free(session->file.file_path);
    } else {
        free(session->ident);
    }

    free(session);
}