/**
 * \file src/plugins/input/udp/udp.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief UDP input plugin for IPFIXcol 2
 * \date 2018-2019
 */

/* Copyright (C) 2018-2019 CESNET, z.s.p.o.
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

#include <ipfixcol2.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include "config.h"

/** Identification of an invalid socket descriptor                                               */
#define INVALID_FD        (-1)
/** Timeout for a getter operation - i.e. epoll_wait timeout [in milliseconds]                   */
#define GETTER_TIMEOUT    (10)
/** Max sockets events processed in the getter - i.e. epoll_wait array size                      */
#define GETTER_MAX_EVENTS (16)
/** Number of seconds between timer events [seconds]                                             */
#define TIMER_INTERVAL    (2)
/** Required minimal size of receive buffer size [bytes] (otherwise produces a warning message)  */
#define UDP_RMEM_REQ      (1024*1024)

/** Plugin description */
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin type
    .type = IPX_PT_INPUT,
    // Plugin identification name
    .name = "udp",
    // Brief description of plugin
    .dsc = "Input plugins for IPFIX/NetFlow v5/v9 over User Datagram Protocol.",
    // Configuration flags (reserved for future use)
    .flags = 0,
    // Plugin version string (like "1.2.3")
    .version = "2.1.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    .ipx_min = "2.1.0"
};

/**
 * @struct nf5_msg_hdr
 * @brief NetFlow v5 Packet Header structure
 * @warning All values are stored in Network Byte Order!
 */
struct __attribute__((__packed__)) nf5_msg_hdr {
    /// NetFlow export format version number
    uint16_t version;
    /// Number of flows exported in this packet (1 - 30)
    uint16_t count;
    /// Current time in milliseconds since the export device booted
    uint32_t sys_uptime;
    /// Current count of seconds since 0000 UTC 1970
    uint32_t unix_sec;
    /// Residual nanoseconds since 0000 UTC 1970
    uint32_t unix_nsec;
    /// Sequence counter of total flows seen
    uint32_t flow_seq;
    /// Type of flow-switching engine
    uint8_t  engine_type;
    /// Slot number of the flow-switching engine
    uint8_t  engine_id;
    /// First two bits hold the sampling mode. Remaining 14 bits hold value of sampling interval
    uint16_t sampling_interval;
};

/** Version identification in NetFlow v5 header                                                  */
#define NF5_HDR_VERSION   (5)
/** Length of NetFlow v5 header (in bytes)                                                       */
#define NF5_HDR_LEN       (sizeof(struct nf5_msg_hdr))

/**
 * @struct nf9_msg_hdr
 * @brief NetFlow v9 Packet Header structure
 * @warning All values are stored in Network Byte Order!
 */
struct __attribute__((__packed__)) nf9_msg_hdr {
    /// Version of Flow Record format exported in this packet
    uint16_t version;
    /// The total number of records in the Export Packet
    uint16_t count;
    /// Time in milliseconds since this device was first booted
    uint32_t sys_uptime;
    /// Time in seconds since 0000 UTC 1970, at which the Export Packet leaves the Exporter
    uint32_t unix_sec;
    /// Incremental sequence counter of all Export Packets by the Exporter
    uint32_t seq_number;
    /// Exporter Observation Domain
    uint32_t source_id;
};

/** Version identification in NetFlow v9 header                                                  */
#define NF9_HDR_VERSION   (9)
/** Length of NetFlow v9 header (in bytes)                                                       */
#define NF9_HDR_LEN       (sizeof(struct nf9_msg_hdr))


/** Description of a UDP Transport Session                                                       */
struct udp_source {
    /** Identification of local socket (on which the data came)                                  */
    int local_fd;
    /** Source (i.e. remote) IP address and port                                                 */
    struct sockaddr_storage src_addr;

    /** Description of  the Transport Session                                                    */
    struct ipx_session *session;

    /** Timestamp of check when the source was last seen                                         */
    time_t last_seen;
    /** Number of received messages since last check                                             */
    uint32_t msg_cnt;
    /** No message has been received from the Session yet                                        */
    bool new_connection;
};

/** Instance data                                                                                */
struct udp_data {
    /** Parsed configuration parameters                                                          */
    struct udp_config *config;
    /** Instance context                                                                         */
    ipx_ctx_t *ctx;

    struct {
        /** Size of the array                                                                    */
        size_t cnt;
        /** Array of sockets                                                                     */
        int *sockets;
        /** New size of receive buffer (try to change only if not equal to zero)                 */
        int rmem_size;

        /** Epoll file descriptor (binded sockets and timer)                                     */
        int epoll_fd;
        /** Timer file descriptor (#INVALID_FD if not valid)                                     */
        int timer_fd;
    } listen; /**< Sockets to listen for data                                                    */

    struct {
        /** Size of the array                                                                    */
        size_t cnt;
        /** Array of active sources (identification and corresponding Transport Session)         */
        struct udp_source **sources;
    } active; /**< Active connections                                                            */
};

// -------------------------------------------------------------------------------------------------

/**
 * \brief Create a new socket, bind it to a local address and enable listening for connections
 *
 * Local address and port is taken from \p addr description.
 * \param[in] ctx      Instance contest
 * \param[in] addr     Local IPv4/IPv6 address and port of the socket(sockaddr_in6 or sockaddr_in)
 * \param[in] addrlen  Size of the address
 * \param[in] ipv6only Accept only IPv6 addresses (only for AF_INET6 and the wildcard address)
 * \param[in] rbuffer  Change the receive buffer size (ignored, if zero or negative)
 * \return On failure returns #INVALID_FD. Otherwise returns valid socket descriptor.
 */
static int
address_bind(ipx_ctx_t *ctx, const struct sockaddr *addr, socklen_t addrlen, bool ipv6only,
    int rbuffer)
{
    sa_family_t family = addr->sa_family;
    assert(family == AF_INET || family == AF_INET6);
    int on = 1, off = 0;
    const char *err_str;

    // Create a socket
    int sd = socket(family, SOCK_DGRAM, IPPROTO_UDP);
    if (sd == INVALID_FD) {
        ipx_strerror(errno, err_str);
        IPX_CTX_ERROR(ctx, "Failed to create a socket: %s", err_str);
        return INVALID_FD;
    }

    // Reusable socket
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
        ipx_strerror(errno, err_str);
        IPX_CTX_WARNING(ctx, "Cannot turn on socket reuse option. It may take a while before "
            "the port can be used again. (error: %s)", err_str);
    }

    // Make sure that IPv6 only is disabled
    if (family == AF_INET6) {
        if (!ipv6only && setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)) == -1) {
            ipx_strerror(errno, err_str);
            IPX_CTX_WARNING(ctx, "Cannot turn off socket option IPV6_V6ONLY. Plugin may not accept "
                "IPv4 connections. (error: %s)", err_str);
        }
        if (ipv6only && setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) == -1) {
            ipx_strerror(errno, err_str);
            IPX_CTX_WARNING(ctx, "Cannot turn on socket option IPV6_V6ONLY. Plugin may accept only "
                "IPv6 connections. (error: %s)", err_str);
        }
    }

    // Get the local IP address as a string and local port
    char addr_str[INET6_ADDRSTRLEN] = {0};
    uint16_t port;

    if (family == AF_INET) { // IPv4
        struct sockaddr_in *addr_v4 = (struct sockaddr_in *) addr;
        assert(addr_v4->sin_family == family);
        inet_ntop(family, &addr_v4->sin_addr, addr_str, INET6_ADDRSTRLEN);
        port = ntohs(addr_v4->sin_port);
    } else { // IPv6
        struct sockaddr_in6 *addr_v6 = (struct sockaddr_in6 *) addr;
        assert(addr_v6->sin6_family == family);
        inet_ntop(family, &addr_v6->sin6_addr, addr_str, INET6_ADDRSTRLEN);
        port = ntohs(addr_v6->sin6_port);
    }

    // Get default size of the receive buffer and try to increase it
    int rmem_def = 0;
    socklen_t rmem_size = sizeof(rmem_def);
    if (getsockopt(sd, SOL_SOCKET, SO_RCVBUF, &rmem_def, &rmem_size) == -1) {
        ipx_strerror(errno, err_str);
        IPX_CTX_WARNING(ctx, "Unable get the default socket receive buffer size. getsockopt() "
            "failed: %s", err_str);
    }

    rmem_def /= 2; // Get not doubled size
    if (rbuffer > 0 && rmem_def < rbuffer) {
        // Change the size of the buffer
        if (setsockopt(sd, SOL_SOCKET, SO_RCVBUF, &rbuffer, sizeof(rbuffer)) == -1) {
            ipx_strerror(errno, err_str);
            IPX_CTX_WARNING(ctx, "Unable to expand the socket receive buffer size (from %d to "
                "%d bytes). Some records may be lost under heavy traffic. setsockopt() failed %s",
                rmem_def, rbuffer, err_str);
        } else {
            IPX_CTX_INFO(ctx, "The socket receive buffer size of a new socket (local IP %s) "
                "enlarged (from %d to %d bytes).", addr_str, rmem_def, rbuffer);
        }
    }

    // Bind
    if (bind(sd, addr, addrlen) == -1) {
        ipx_strerror(errno, err_str);
        IPX_CTX_ERROR(ctx, "Cannot bind to a socket (local IP: %s, port %" PRIu16 "): %s",
            addr_str, port, err_str);
        close(sd);
        return INVALID_FD;
    }

    IPX_CTX_INFO(ctx, "Bind succeed on %s (port %" PRIu16 ")", addr_str, port);
    return sd;
}

/**
 * \brief Bind on all local IP address specified in parsed configuration
 *
 * If the no local IP address is defined, the function create one wildcard socket to listen on
 * all local interfaces. Otherwise for each specified address create a separated socket.
 * All sockets are stored into the array of the instance configuration and epoll with added
 * socket is created.
 *
 * \param[in] instance Instance data
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED on failure (typically failed to bind to a port)
 */
static int
listener_bind(struct udp_data *instance)
{
    // Create a poll and new array of binded sockets
    const char *err_str;
    const size_t socket_cnt = instance->config->local_addrs.cnt;
    int *sockets = malloc(sizeof(*sockets) * ((socket_cnt == 0) ? 1 : socket_cnt));
    if (!sockets) {
        IPX_CTX_ERROR(instance->ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        return IPX_ERR_DENIED;
    }

    if (socket_cnt == 0) {
        // Wildcard (i.e. bind to all IPv4 and IPv6 addresses)
        struct sockaddr_in6 addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(instance->config->local_port);
        addr.sin6_addr = in6addr_any;

        int sd = address_bind(instance->ctx, (struct sockaddr *) &addr, sizeof(addr), false,
            instance->listen.rmem_size);
        if (sd == INVALID_FD) {
            free(sockets);
            return IPX_ERR_DENIED;
        }

        // Add the socket to the poll
        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN;
        ev.data.fd = sd;
        if (epoll_ctl(instance->listen.epoll_fd, EPOLL_CTL_ADD, sd, &ev) == -1) {
            ipx_strerror(errno, err_str);
            IPX_CTX_ERROR(instance->ctx, "Failed to add a socket to epoll: %s", err_str);
            close(sd);
            free(sockets);
            return IPX_ERR_DENIED;
        }

        sockets[0] = sd;
        instance->listen.sockets = sockets;
        instance->listen.cnt = 1; // Only this one
        return IPX_OK;
    }

    // Bind to selected local addresses
    size_t idx;
    for (idx = 0; idx < socket_cnt; ++idx) {
        // Create a container
        union {
            struct sockaddr_in v4;
            struct sockaddr_in6 v6;
        } addr_helper;
        memset(&addr_helper, 0, sizeof(addr_helper));
        socklen_t addrlen;
        bool ipv6only;

        const struct udp_ipaddr_rec *rec = &instance->config->local_addrs.addrs[idx];
        if (rec->ip_ver == AF_INET) { // IPv4
            addr_helper.v4.sin_family = AF_INET;
            addr_helper.v4.sin_port = htons(instance->config->local_port);
            addr_helper.v4.sin_addr = rec->ipv4;
            addrlen = sizeof(addr_helper.v4);
            ipv6only = false;
        } else { // IPv6
            addr_helper.v6.sin6_family = AF_INET6;
            addr_helper.v6.sin6_port = htons(instance->config->local_port);
            addr_helper.v6.sin6_addr = rec->ipv6;
            addrlen = sizeof(addr_helper.v6);
            ipv6only = true;
        }

        int sd = address_bind(instance->ctx, (struct sockaddr *) &addr_helper, addrlen, ipv6only,
            instance->listen.rmem_size);
        if (sd == INVALID_FD) {
            // Failed
            break;
        }

        // Add the socket to the poll
        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN;
        ev.data.fd = sd;
        if (epoll_ctl(instance->listen.epoll_fd, EPOLL_CTL_ADD, sd, &ev) == -1) {
            ipx_strerror(errno, err_str);
            IPX_CTX_ERROR(instance->ctx, "Failed to add a socket to epoll: %s", err_str);
            close(sd);
            break;
        }

        sockets[idx] = sd;
    }

    if (idx != socket_cnt) {
        // Something went wrong -> close already initialized sockets
        for (size_t rev = 0; rev < idx; ++rev) {
            close(sockets[rev]);
        }
        free(sockets);
        return IPX_ERR_DENIED;
    }

    instance->listen.sockets = sockets;
    instance->listen.cnt = socket_cnt;
    return IPX_OK;
}

/**
 * \brief Close all binded local IP addresses
 *
 * All binded sockets are deregistered from the epoll instance and closed.
 * \param[in] instance Instance data
 */
static void
listener_unbind(struct udp_data *instance)
{
    for (size_t i = 0; i < instance->listen.cnt; ++i) {
        // Remove from epoll and close
        int sd = instance->listen.sockets[i];
        epoll_ctl(instance->listen.epoll_fd, EPOLL_CTL_DEL, sd, NULL);
        close(sd);
    }

    free(instance->listen.sockets);
    instance->listen.sockets = NULL;
    instance->listen.cnt = 0;
}

/**
 * \brief Initialize local IP addresses to listen on
 *
 * Based on parsed configuration, local IP addresses are binded and a timer that regularly checks
 * inactive connections is armed.
 * \param[in] instance Instance data
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED on failure (typically failed to bind to a port)
 */
static int
listener_init(struct udp_data *instance)
{
    const char *err_str;

    // Get maximum socket receive buffer size (in bytes)
    FILE *f;
    int sock_rmax = 0;
    static const char *sys_cfg = "/proc/sys/net/core/rmem_max";
    if ((f = fopen(sys_cfg, "r")) == NULL || fscanf(f, "%d", &sock_rmax) != 1 || sock_rmax < 0) {
        ipx_strerror(errno, err_str);
        IPX_CTX_WARNING(instance->ctx, "Unable to get the maximum socket receive buffer size "
            "from '%s' (%s). Due to potentially small buffers, some records may be lost!", sys_cfg,
            err_str);
        sock_rmax = 0;
    }

    instance->listen.rmem_size = sock_rmax;
    if (f != NULL) {
        fclose(f);
    }

    if (sock_rmax != 0 && sock_rmax < UDP_RMEM_REQ) {
        IPX_CTX_WARNING(instance->ctx, "The maximum socket receive buffer size is too small "
            "(%d bytes). Some records may be lost under heavy traffic. See documentation for "
            "more details!", sock_rmax);
    }

    // Create epoll and bind all sockets
    instance->listen.epoll_fd = epoll_create(1);
    if (instance->listen.epoll_fd == INVALID_FD) {
        ipx_strerror(errno, err_str);
        IPX_CTX_ERROR(instance->ctx, "epoll() failed: %s", err_str);
        return IPX_ERR_DENIED;
    }

    if (listener_bind(instance) != IPX_OK) {
        close(instance->listen.epoll_fd);
        return IPX_ERR_DENIED;
    }

    // Create a timer and arms it
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timer_fd == INVALID_FD) {
        ipx_strerror(errno, err_str);
        IPX_CTX_ERROR(instance->ctx, "Failed to create a timer. timerfd_create() failed: %s",
            err_str);
        listener_unbind(instance);
        close(instance->listen.epoll_fd);
        return EXIT_FAILURE;
    }

    struct itimerspec spec;
    memset(&spec, 0, sizeof(spec));
    spec.it_interval.tv_sec = TIMER_INTERVAL;
    spec.it_interval.tv_nsec = 0;
    spec.it_value.tv_sec = TIMER_INTERVAL;
    spec.it_value.tv_nsec = 0;

    if (timerfd_settime(timer_fd, 0, &spec, NULL) == -1) {
        ipx_strerror(errno, err_str);
        IPX_CTX_ERROR(instance->ctx, "Failed to arm a timer. timerfd_settime() failed: %s", err_str);
        close(timer_fd);
        listener_unbind(instance);
        close(instance->listen.epoll_fd);
        return EXIT_FAILURE;
    }

    // Add the timer on the epoll instance
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = timer_fd;
    if (epoll_ctl(instance->listen.epoll_fd, EPOLL_CTL_ADD, timer_fd, &ev) == -1) {
        ipx_strerror(errno, err_str);
        IPX_CTX_ERROR(instance->ctx, "Failed to add a timer to epoll: %s", err_str);
        close(timer_fd);
        listener_unbind(instance);
        close(instance->listen.epoll_fd);
        return EXIT_FAILURE;
    }

    instance->listen.timer_fd = timer_fd;
    return IPX_OK;
}

/**
 * \brief Destroy the listener structure of the instance
 *
 * All acceptor sockets are closed and unregistered from epoll which is also destroyed. The timer
 * is disarmed and destroyed too.
 * \param[in] instance Instance data
 */
static void
listener_destroy(struct udp_data *instance)
{
    // Close all sockets
    listener_unbind(instance);
    // Destroy the timer and epoll
    close(instance->listen.epoll_fd);
    close(instance->listen.timer_fd);
}

/**
 * \brief Add a new record of a Transport Session
 *
 * New record is added into the list of active connections.
 * \param[in] instance Instance data
 * \param[in] src_fd   Socket descriptor of local address on which the source data come
 * \param[in] src_addr Remote IPv4/IPv6 address to add
 * \return Pointer to the newly added record or NULL (memory allocation error)
 */
static struct udp_source *
active_add(struct udp_data *instance, int src_fd, const struct sockaddr *src_addr)
{
    socklen_t src_addrlen;

    // Get the description of the local address
    struct sockaddr_storage dst_addr;
    socklen_t dst_addrlen = sizeof(dst_addr);
    memset(&dst_addr, 0, sizeof(dst_addr));

    if (getsockname(src_fd, (struct sockaddr *) &dst_addr, &dst_addrlen) == -1) {
        const char *err_str;
        ipx_strerror(errno, err_str);
        IPX_CTX_ERROR(instance->ctx, "Failed to get the local IP address. getsockname() failed: %s",
            err_str);
        return NULL;
    }

    if (src_addr->sa_family != dst_addr.ss_family) {
        IPX_CTX_WARNING(instance->ctx, "New connection has different family of local and "
            "remote IP addresses!", '\0');
    }

    // Create a new Transport Session
    struct ipx_session_net net;
    memset(&net, 0, sizeof(net));

    net.l3_proto = src_addr->sa_family;
    if (net.l3_proto == AF_INET) { // IPv4
        src_addrlen = sizeof(struct sockaddr_in);
        const struct sockaddr_in *src_v4 = (struct sockaddr_in *) src_addr;
        const struct sockaddr_in *dst_v4 = (struct sockaddr_in *) &dst_addr;
        net.port_src = ntohs(src_v4->sin_port);
        net.port_dst = ntohs(dst_v4->sin_port);
        net.addr_src.ipv4 = src_v4->sin_addr;
        net.addr_dst.ipv4 = dst_v4->sin_addr;
    } else if (net.l3_proto == AF_INET6) { // IPv6
        src_addrlen = sizeof(struct sockaddr_in6);
        const struct sockaddr_in6 *src_v6 = (struct sockaddr_in6 *) src_addr;
        const struct sockaddr_in6 *dst_v6 = (struct sockaddr_in6 *) &dst_addr;
        net.port_src = ntohs(src_v6->sin6_port);
        net.port_dst = ntohs(dst_v6->sin6_port);
        if (IN6_IS_ADDR_V4MAPPED(&src_v6->sin6_addr)) {
            // IPv4 mapped into IPv6
            net.l3_proto = AF_INET; // Overwrite family type!
            net.addr_src.ipv4 = *(const struct in_addr *) (((uint8_t *) &src_v6->sin6_addr) + 12);
            net.addr_dst.ipv4 = *(const struct in_addr *) (((uint8_t *) &dst_v6->sin6_addr) + 12);
        } else {
            net.addr_src.ipv6 = src_v6->sin6_addr;
            net.addr_dst.ipv6 = dst_v6->sin6_addr;
        }
    } else {
        // Unknown address type
        IPX_CTX_ERROR(instance->ctx, "New connection has an unsupported IP address family "
            "(family ID: %u)!", (unsigned) src_addr->sa_family);
        return NULL;
    }

    char src_addr_str[INET6_ADDRSTRLEN] = {0};
    inet_ntop(net.l3_proto, &net.addr_src, src_addr_str, INET6_ADDRSTRLEN);

    const struct udp_config *cfg = instance->config;
    struct ipx_session *session = ipx_session_new_udp(&net, cfg->lifetime_data, cfg->lifetime_opts);
    if (!session) {
        IPX_CTX_ERROR(instance->ctx, "Failed to create a Transport Session description of %s.",
            src_addr_str);
        return NULL;
    }

    // Create a new record
    struct udp_source *rec2add = calloc(1, sizeof(*rec2add));
    if (!rec2add) {
        IPX_CTX_ERROR(instance->ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        ipx_session_destroy(session);
        return NULL;
    }

    rec2add->local_fd = src_fd;
    memcpy(&rec2add->src_addr, src_addr, src_addrlen);
    rec2add->session = session;
    rec2add->last_seen = time(NULL); // now!
    rec2add->msg_cnt = 0;
    rec2add->new_connection = true; // Session Message hasn't been send yet

    // Append the list of active connections
    const size_t new_size = (instance->active.cnt + 1) * sizeof(struct udp_source *);
    struct udp_source **new_sources = realloc(instance->active.sources, new_size);
    if (!new_sources) {
        IPX_CTX_ERROR(instance->ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        free(rec2add);
        ipx_session_destroy(session);
        return NULL;
    }

    IPX_CTX_INFO(instance->ctx, "New exporter connected from '%s'.", src_addr_str);
    new_sources[instance->active.cnt] = rec2add;
    instance->active.sources = new_sources;
    instance->active.cnt++;
    return rec2add;
}

/**
 * \brief Remove an active Transport Session with a given index
 *
 * Generate and pass a Session Message - connect event (if necessary) and remove the corresponding
 * session (defined by an index) from the list.
 * \note After removing of the Session the array of active Transport Sessions is not order by
 *   activity anymore.
 * \param[in] instance Instance data
 * \param[in] idx      Index of the instance to remove
 */
static void
active_remove_by_id(struct udp_data *instance, size_t idx)
{
    struct udp_source *src = instance->active.sources[idx];
    IPX_CTX_INFO(instance->ctx, "Transport Session '%s' closed!", src->session->ident);

    // Have we received at least one valid record?
    if (src->new_connection) {
        // No messages have been passed with a reference to this session -> destroy immediately
        ipx_session_destroy(src->session);
    } else {
        // Generate a Session message (order of the messages MUST be preserved)
        ipx_msg_session_t *msg_sess = ipx_msg_session_create(src->session, IPX_MSG_SESSION_CLOSE);
        if (!msg_sess) {
            IPX_CTX_WARNING(instance->ctx, "Failed to create a Session message! Instances of "
                "plugins will not be informed about the closed Transport Session '%s' (%s:%d)",
                src->session->ident, __FILE__, __LINE__);
            /* Do not pass and definitely do NOT free the session structure because it still can be
             * used by other plugins. Remove it only from the local table!
             * NO RETURN HERE!
             */
        } else {
            // Pass the message and put the Session into the garbage
            ipx_ctx_msg_pass(instance->ctx, ipx_msg_session2base(msg_sess));

            ipx_msg_garbage_cb cb = (ipx_msg_garbage_cb) &ipx_session_destroy;
            ipx_msg_garbage_t *msg_garbage = ipx_msg_garbage_create(src->session, cb);
            if (!msg_garbage) {
                IPX_CTX_ERROR(instance->ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
            } else {
                ipx_ctx_msg_pass(instance->ctx, ipx_msg_garbage2base(msg_garbage));
            }
        }
    }

    // Now we can free the wrapper
    free(src);
    if (idx == instance->active.cnt - 1) {
        // The last element in the array
        instance->active.cnt--;
    } else {
        // Replace it with the last one
        instance->active.sources[idx] = instance->active.sources[instance->active.cnt - 1];
        instance->active.cnt--;
    }
}

/**
 * \brief Find a record of an active Transport Session
 *
 * \param[in] instance Instance data
 * \param[in] src_fd   Socket descriptor of local address on which the source data come
 * \param[in] addr     Remote IPv4/IPv6 Address to find
 * \return Pointer to the record or NULL (the record doesn't exist)
 */
static struct udp_source *
active_find(struct udp_data *instance, int src_fd, const struct sockaddr *addr)
{
    size_t idx;

    // The array is not sorted, we have to go through
    for (idx = 0; idx < instance->active.cnt; ++idx) {
        struct udp_source *src = instance->active.sources[idx];
        if (src->local_fd != src_fd) {
            continue; // Different local socket
        }

        if (src->src_addr.ss_family != addr->sa_family) {
            continue; // Different IP address family (IPv4 vs IPv6)
        }

        if (addr->sa_family == AF_INET) {
            // IPv4 addresses
            const struct sockaddr_in *to_find = (const struct sockaddr_in *) addr;
            const struct sockaddr_in *to_cmp = (const struct sockaddr_in *) &src->src_addr;
            if (to_find->sin_port != to_cmp->sin_port) {
                continue; // Different ports
            }

            if (memcmp(&to_find->sin_addr, &to_cmp->sin_addr, sizeof(struct in_addr)) != 0) {
                continue;
            }
            // Found
            break;
        } else {
            // IPv6 addresses
            assert(addr->sa_family == AF_INET6);
            const struct sockaddr_in6 *to_find = (const struct sockaddr_in6 *) addr;
            const struct sockaddr_in6 *to_cmp = (const struct sockaddr_in6 *) &src->src_addr;
            if (to_find->sin6_port != to_cmp->sin6_port) {
                continue; // Different ports
            }

            if (memcmp(&to_find->sin6_addr, &to_cmp->sin6_addr, sizeof(struct in6_addr)) != 0) {
                continue;
            }
            // Found
            break;
        }
    }

    if (idx == instance->active.cnt) {
        // Not found
        return NULL;
    }

    return instance->active.sources[idx];
}

/**
 * \brief Get a reference to a Transport Session
 *
 * First, try to find in among already active Transport Sessions. If it is not present, create
 * a new one and store it into the array of active Sessions.
 * \param[in] instance Instance data
 * \param[in] src_fd   Socket descriptor to which is the Session connected
 * \param[in] addr     Remove IPv4/IPv6 address (and port) of the session
 * \return Pointer to the Session or NULL (typically memory allocation error)
 */
static struct udp_source *
active_get(struct udp_data *instance, int src_fd, const struct sockaddr *addr)
{
    // Try to find
    struct udp_source *src = active_find(instance, src_fd, addr);
    if (src != NULL) {
        return src;
    }

    // Not found, add a new record
    return active_add(instance, src_fd, addr);
}

/**
 * \brief Compare 2 active Transport Session by activity
 *
 * \note The sessions are primary sorted by number of messages received since the last reset of
 * counters. Secondary, if the both counters are zero, the sessions are sorted by the timestamp
 * of their last occurrence.
 * \param p1 First Transport Session
 * \param p2 Second Transport Session
 * \return
 *   The comparison function returns an integer less than, equal to, or greater than zero if the
 *   first Session is considered to be respectively more, equal to, or less active than the
 *   second.
 */
static int
active_cmp_activity(const void *p1, const void *p2)
{
    const struct udp_source *left =  *(const struct udp_source **) p1;
    const struct udp_source *right = *(const struct udp_source **) p2;

    // Primary, compare by number of messages since the last reset of counter
    if (left->msg_cnt != right->msg_cnt) {
        return (left->msg_cnt > right->msg_cnt) ? (-1) : 1; // Sort in descending order
    }

    if (left->msg_cnt != 0) {
        // Both have the same non-zero value of counters,
        return 0;
    }

    assert(left->msg_cnt == 0 && right->msg_cnt == 0);
    // Secondary, compare by the last seen timestamps
    if (left->last_seen == right->last_seen) {
        return 0;
    }

    return (left->last_seen > right->last_seen) ? (-1) : 1;
}

/**
 * \brief Sort all active Transport Session by activity and reset activity counters
 *
 * The most active Transport Session go to the front. Activity is compared by number of received
 * IPFIX Messages since the last restart of counters.
 * \param[in] instance Instance data
 */
static void
active_sort_and_reset(struct udp_data *instance)
{
    const size_t rec_size = sizeof(*instance->active.sources);
    qsort(instance->active.sources, instance->active.cnt, rec_size, &active_cmp_activity);

    time_t now = time(NULL);
    for (size_t i = 0; i < instance->active.cnt; ++i) {
        struct udp_source *src = instance->active.sources[i];
        if (src->msg_cnt == 0) {
            // No new messages since the last check
            continue;
        }

        src->last_seen = now;
        src->msg_cnt = 0;
    }
}

/**
 * \brief Process a timer event
 *
 * Sort all sources by their activity (number of messages sent between timer events) and
 * close inactive Transport Sessions.
 * \param[in] instance Instance data
 * \param[in] fd       File descriptor of a timer
 */
static void
process_timer(struct udp_data *instance, int fd)
{
    assert(fd == instance->listen.timer_fd);
    // Read the event
    uint64_t event_cnt;
    ssize_t ret = read(fd, &event_cnt, sizeof(event_cnt));
    if (ret == -1 || ((size_t) ret) != sizeof(event_cnt)) {
        int error_code = (ret == -1) ? errno : EINTR;
        const char *err_str;
        ipx_strerror(error_code, err_str);
        IPX_CTX_ERROR(instance->ctx, "Unable to get status of a timer, read() failed: %s", err_str);
        return;
    }

    // The inactive Transport Sessions will be at the end of the (sorted) array
    active_sort_and_reset(instance);

    time_t now = time(NULL);
    size_t idx;
    for (idx = 0; idx < instance->active.cnt; ++idx) {
        if (instance->active.sources[idx]->last_seen + instance->config->timeout_conn >= now) {
            continue;
        }

        // Close this Session and all that follows
        break;
    }

    while (idx < instance->active.cnt) {
        // Remove and generate Session message - close event, if necessary
        assert(instance->active.sources[idx]->last_seen + instance->config->timeout_conn < now);
        active_remove_by_id(instance, idx);
    }

    IPX_CTX_DEBUG(instance->ctx, "The instance holds information about %zu active session(s).",
        instance->active.cnt);
}

/**
 * \brief Get an IPFIX/NetFlow message from a socket and pass it
 * \param[in] instance Instance data
 * \param[in] sd       File descriptor of the socket
 */
static void
process_socket(struct udp_data *instance, int sd)
{
    const char *err_str;

    // Get size of the message
    int msg_size;
    if (ioctl(sd, FIONREAD, &msg_size) == -1) {
        ipx_strerror(errno, err_str);
        IPX_CTX_ERROR(instance->ctx, "Unable to get size of a next datagram. ioctl() failed: %s",
            err_str);
        return;
    }

    if (msg_size < (int) sizeof(uint16_t) || msg_size > UINT16_MAX) {
        // Remove the malformed message from the buffer
        uint16_t mini_buffer;
        ssize_t ret = recvfrom(sd, &mini_buffer, sizeof(mini_buffer), 0, NULL, NULL);
        if (ret == -1) {
            ipx_strerror(errno, err_str);
            IPX_CTX_WARNING(instance->ctx, "An error has occurred during reading a malformed "
                "message. recvfrom() failed: %s", err_str);
            return;
        }

        IPX_CTX_WARNING(instance->ctx, "Received an invalid datagram (%d bytes long)", msg_size);
        return;
    }

    // Allocate the buffer
    uint8_t *buffer = malloc(msg_size * sizeof(uint8_t));
    if (!buffer) {
        IPX_CTX_ERROR(instance->ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        return;
    }

    // Get the message
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    ssize_t ret = recvfrom(sd, buffer, (size_t) msg_size, 0, (struct sockaddr *) &addr, &addr_len);
    if (ret == -1) {
        // Failed
        ipx_strerror(errno, err_str);
        IPX_CTX_ERROR(instance->ctx, "Failed to read a datagram. recvfrom() failed %s", err_str);
        free(buffer);
        return;
    }

    if (ret != msg_size) {
        IPX_CTX_ERROR(instance->ctx, "Read operation failed! Got %zu of %zu bytes!",
            (size_t) ret, (size_t) msg_size);
        free(buffer);
        return;
    }

    // Find the source
    struct udp_source *source = active_get(instance, sd, (struct sockaddr *) &addr);
    if (!source) { // Memory allocation error!
        free(buffer);
        return;
    }

    // Check NetFlow/IPFIX header length and extract ODID/Source ID
    const uint16_t msg_ver = ntohs(*(uint16_t *) buffer);
    uint32_t msg_odid = 0;
    bool is_len_ok = true;

    switch (msg_ver) {
    case FDS_IPFIX_VERSION: // IPFIX
        if (msg_size < FDS_IPFIX_MSG_HDR_LEN) {
            is_len_ok = false;
            break;
        }

        msg_odid = ntohl(((const struct fds_ipfix_msg_hdr *) buffer)->odid);
        break;
    case NF9_HDR_VERSION: // NetFlow v9
        if (msg_size < (int) NF9_HDR_LEN) {
            is_len_ok = false;
            break;
        }

        msg_odid = ntohl(((const struct nf9_msg_hdr *) buffer)->source_id);
        break;
    case NF5_HDR_VERSION: // NetFlow v5
        if (msg_size < (int) NF5_HDR_LEN) {
            is_len_ok = false;
            break;
        }

        // Source ID is not available in NetFlow v5 -> always 0
        msg_odid = 0;
        break;
    default:
        is_len_ok = false;
        break;
    }

    if (!is_len_ok) {
        IPX_CTX_ERROR(instance->ctx, "Receiver an invalid NetFlow/IPFIX Message header from '%s'. "
            "The message will be dropped!", source->session->ident);
        free(buffer);
        return;
    }

    if (source->new_connection) {
        // Send information about the new Transport Session
        source->new_connection = false;
        ipx_msg_session_t *msg = ipx_msg_session_create(source->session, IPX_MSG_SESSION_OPEN);
        if (!msg) {
            IPX_CTX_WARNING(instance->ctx, "Failed to create a Session message! Instances of "
                "plugins will not be informed about the new Transport Session '%s' (%s:%d).",
                source->session->ident, __FILE__, __LINE__);
        } else {
            ipx_ctx_msg_pass(instance->ctx, ipx_msg_session2base(msg));
        }
    }

    // Create a message wrapper and pass the message
    struct ipx_msg_ctx msg_ctx;
    msg_ctx.session = source->session;
    msg_ctx.odid = msg_odid;
    msg_ctx.stream = 0; // Streams are not supported over UDP

    ipx_msg_ipfix_t *msg = ipx_msg_ipfix_create(instance->ctx, &msg_ctx, buffer, (uint16_t) msg_size);
    if (!msg) {
        IPX_CTX_ERROR(instance->ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        free(buffer);
        return;
    }

    ipx_ctx_msg_pass(instance->ctx, ipx_msg_ipfix2base(msg));
    source->msg_cnt++;
}

// -------------------------------------------------------------------------------------------------

int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params)
{
    struct udp_data *data = calloc(1, sizeof(*data));
    if (!data) {
        IPX_CTX_ERROR(ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        return IPX_ERR_DENIED;
    }

    data->ctx = ctx;
    data->active.cnt = 0;
    data->active.sources = NULL;

    // Parse configuration
    data->config = config_parse(ctx, params);
    if (!data->config) {
        free(data);
        return IPX_ERR_DENIED;
    }

    // Bind to local addresses and arm a timer
    if (listener_init(data) != IPX_OK) {
        config_destroy(data->config);
        free(data);
        return IPX_ERR_DENIED;
    }

    ipx_ctx_private_set(ctx, data);
    return IPX_OK;
}

void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg)
{
    (void) ctx;
    struct udp_data *data = (struct udp_data *) cfg;
    // Unbind all local IP addresses and disarm the timer
    listener_destroy(data);

    // Close all Transport Session (this generates Session messages per each active Session)
    while (data->active.cnt > 0) {
        active_remove_by_id(data, 0);
    }
    free(data->active.sources);

    config_destroy(data->config);
    free(data);
}

int
ipx_plugin_get(ipx_ctx_t *ctx, void *cfg)
{
    struct udp_data *data = (struct udp_data *) cfg;


    // Process messages from up to 16 sockets (including the timer)
    struct epoll_event ev[GETTER_MAX_EVENTS];
    int ev_valid = epoll_wait(data->listen.epoll_fd, ev, GETTER_MAX_EVENTS, GETTER_TIMEOUT);
    if (ev_valid == -1) {
        // Failed
        int error_code = errno;
        const char *err_str;
        ipx_strerror(error_code, err_str);
        IPX_CTX_ERROR(ctx, "epoll_wait() failed: %s", err_str);
        if (error_code == EINTR) {
            return IPX_OK;
        }
        // Fatal error -> stop the plugin
        return IPX_ERR_DENIED;
    }

    if (ev_valid == 0) {
        // Timeout
        return IPX_OK;
    }

    // Process all events
    assert(ev_valid > 0 && ev_valid <= GETTER_MAX_EVENTS);
    for (int i = 0; i < ev_valid; ++i) {
        int sd = ev[i].data.fd;

        if (sd == data->listen.timer_fd) {
            // Timer event
            process_timer(data, sd);
            continue;
        }

        process_socket(data, sd);
    }

    return IPX_OK;
}
