/**
 * \file src/plugins/input/tcp/tcp.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief TCP input plugin for IPFIXcol 2
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

#include <ipfixcol2.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <inttypes.h>
#include "config.h"

/** Identification of an invalid socket descriptor                                               */
#define INVALID_FD        (-1)
/** Number of outstanding connections in the socket's listen queue                               */
#define LISTEN_BACKLOG    (SOMAXCONN)
/** Timeout for a getter operation - i.e. epoll_wait timeout (in milliseconds)                   */
#define GETTER_TIMEOUT    (10)
/** Max sockets events processed in the getter - i.e. epoll_wait array size                      */
#define GETTER_MAX_EVENTS (16)
/** Timeout to read whole IPFIX Message after at least part has been received (in microseconds)  */
#define GETTER_RECV_TIMEOUT (500000)
/** Default size of a buffer prepared for new IPFIX/NetFlow message (bytes)                      */
#define DEF_MSG_SIZE      (4096)

/** Plugin description */
IPX_API struct ipx_plugin_info ipx_plugin_info = {
    // Plugin type
    .type = IPX_PT_INPUT,
    // Plugin identification name
    .name = "tcp",
    // Brief description of plugin
    .dsc = "Input plugins for IPFIX/NetFlow v5/v9 over Transmission Control Protocol.",
    // Configuration flags (reserved for future use)
    .flags = 0,
    // Plugin version string (like "1.2.3")
    .version = "2.0.0",
    // Minimal IPFIXcol version string (like "1.2.3")
    .ipx_min = "2.0.0"
};

/** Auxiliary combination of a file descriptor and corresponding Transport Session               */
struct tcp_pair {
    /** File descriptor of the Transport Session                                                 */
    int fd;
    /** Description of  the Transport Session                                                    */
    struct ipx_session *session;
    /** No message has been received from the Session yet                                        */
    bool new_connection;
};

/** Instance data                                                                                */
struct tcp_data {
    /** Parsed configuration of the plugin                                                       */
    struct tcp_config *config;
    /** Reference to the plugin context                                                          */
    ipx_ctx_t *ctx;

    struct {
        /** Size of the array                                                                    */
        size_t cnt;
        /** Array of sockets                                                                     */
        int *sockets;

        /** Epoll file descriptor                                                                */
        int epoll_fd;
        /** Acceptor thread                                                                      */
        pthread_t thread;
    } listen; /**< Sockets to lister for new connections                                         */

    struct {
        /** Size of the array                                                                    */
        size_t cnt;
        /** Array of pairs (a file descriptor and a corresponding Transport Session)             */
        struct tcp_pair **pairs;
        /** Protection of the array modification (adding/removing)                               */
        pthread_mutex_t lock;

        /** Epoll file descriptor                                                                */
        int epoll_fd;
    } active; /**< Active connections                                                            */
};

/**
 * \brief Add a session into a list of active Transport Session
 *
 * The function creates for a file descriptor and a Transport Session new pair that is inserted
 * into the list of active sessions. The file descriptor is also registered on epoll instance
 * of active connections.
 *
 * \param[in] data    Instance data
 * \param[in] sd      Socket descriptor of the Transport Session
 * \param[in] session Description of the Transport Session
 * \return #IPX_OK on success (the pair is added and the socket is registered)
 * \return #IPX_ERR_NOMEM in case of a memory allocation error
 * \return #IPX_ERR_DENIED if the epoll failed to register the socket and the pair is not added
 */
static int
active_session_add(struct tcp_data *data, int sd, struct ipx_session *session)
{
    // Create a new pair
    struct tcp_pair *pair = malloc(sizeof(*pair));
    if (!pair) {
        IPX_CTX_ERROR(data->ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }
    pair->fd = sd;
    pair->session = session;
    pair->new_connection = true;

    pthread_mutex_lock(&data->active.lock);

    // Append the list
    size_t new_size = (data->active.cnt + 1) * sizeof(*data->active.pairs);
    struct tcp_pair **new_pairs = realloc(data->active.pairs, new_size);
    if (!new_pairs) {
        pthread_mutex_unlock(&data->active.lock);
        IPX_CTX_ERROR(data->ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        free(pair);
        return IPX_ERR_NOMEM;
    }

    new_pairs[data->active.cnt] = pair;
    data->active.pairs = new_pairs;
    data->active.cnt++;

    // Add the session to the poll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = pair; // Pointer to the pair instead of FD
    if (epoll_ctl(data->active.epoll_fd, EPOLL_CTL_ADD, sd, &ev) == -1) {
        // Failed to register the socket
        const char *err_str;
        ipx_strerror(errno, err_str);
        data->active.cnt--;
        pthread_mutex_unlock(&data->active.lock);
        IPX_CTX_ERROR(data->ctx, "Unable to register a Transport Session. epoll_ctl() failed: %s",
            err_str);
        free(pair);
        return IPX_ERR_DENIED;
    }

    pthread_mutex_unlock(&data->active.lock);
    return IPX_OK;
}

/**
 * \brief Remove a session for a list of active Transport Session (auxiliary function)
 *
 * Remove (deregister) the session on the epoll instance of active connections. Generate and pass
 * a Session Message - connect event (if necessary), close the socket and remove the corresponding
 * pair (defined by an index) from the list.
 * \warning The list MUST be locked before calling this function!
 * \param[in] data Instance data
 * \param[in] idx  Index of the pair (socket descriptor and session) to remove
 */
static void
active_session_remove_aux(struct tcp_data *data, size_t idx)
{
    assert(idx < data->active.cnt);
    struct tcp_pair *pair = data->active.pairs[idx];
    IPX_CTX_INFO(data->ctx, "Closing a connection from '%s'.", pair->session->ident);

    // Remove from poll
    if (epoll_ctl(data->active.epoll_fd, EPOLL_CTL_DEL, pair->fd, NULL) == -1) {
        const char *err_str;
        ipx_strerror(errno, err_str);
        IPX_CTX_WARNING(data->ctx, "Failed to deregister the Transport Session of %s. "
            "epoll_ctl failed: %s", pair->session->ident, err_str);
    }

    // Have we received at least one message?
    if (pair->new_connection) {
        // No messages with a reference to the session -> destroy it immediately
        ipx_session_destroy(pair->session);
    } else {
        // Generate a Session message (order of the messages MUST be preserved)
        ipx_msg_session_t *msg_sess = ipx_msg_session_create(pair->session, IPX_MSG_SESSION_CLOSE);
        if (!msg_sess) {
            IPX_CTX_WARNING(data->ctx, "Failed to create a Session message! Instances of plugins "
                "will not be informed about the closed Transport Session '%s' (%s:%d)",
                pair->session->ident, __FILE__, __LINE__);
            /* Do not pass and definitely do NOT free the session structure because it still can be
             * used by other plugins. Remove it only from the local table!
             * NO RETURN HERE!
             */
        } else {
            // Pass the message and put the Session into the garbage
            ipx_ctx_msg_pass(data->ctx, ipx_msg_session2base(msg_sess));

            ipx_msg_garbage_cb cb = (ipx_msg_garbage_cb) &ipx_session_destroy;
            ipx_msg_garbage_t *msg_garbage = ipx_msg_garbage_create(pair->session, cb);
            if (!msg_garbage) {
                IPX_CTX_ERROR(data->ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
            } else {
                ipx_ctx_msg_pass(data->ctx, ipx_msg_garbage2base(msg_garbage));
            }
        }
    }

    // Close internal structures an remove it from the list (do NOT free SESSION)
    close(pair->fd);
    free(pair);

    if (idx == data->active.cnt - 1) {
        // The last element in the array
        data->active.cnt--;
    } else {
        // Replace it with the last
        data->active.pairs[idx] = data->active.pairs[data->active.cnt - 1];
        data->active.cnt--;
    }
}

/**
 * \brief Remove a session for a list of active Transport Session
 *
 * Close sockets, deregister on the epoll instance, send a Session Message (close event)
 * \param[in] data     Instance data
 * \param[in] session  Session to remove
 * \return #IPX_OK on success
 * \return #IPX_ERR_NOTFOUND if the session is not present in the list
 */
static int
active_session_remove_by_session(struct tcp_data *data, const struct ipx_session *session)
{
    pthread_mutex_lock(&data->active.lock);
    // Try to find the pair
    size_t i;
    for (i = 0; i < data->active.cnt; ++i) {
        struct tcp_pair *pair = data->active.pairs[i];
        if (pair->session != session) {
            continue;
        }

        // Found
        break;
    }

    if (i == data->active.cnt) {
        // Not found
        pthread_mutex_unlock(&data->active.lock);
        return IPX_ERR_NOTFOUND;
    }

    // Found
    active_session_remove_aux(data, i); // close session, generate a Session message, etc.
    pthread_mutex_unlock(&data->active.lock);
    return IPX_OK;
}

/**
 * \brief Add a new connection
 *
 * Socket parameters are configured for the socket (such as receive timeout), the connection
 * is inserted into active connections and registered on the epoll instance of active connections.
 * \param[in] data Instance data
 * \param[in] sd   Socket descriptor to add
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED on failure (the socket should be closed by the user)
 */
static int
listener_add_connection(struct tcp_data *data, int sd)
{
    assert(sd >= 0);
    const char *err_str;

    // Set receive timeout (after data on the socket is ready)
    struct timeval rcv_timeout;
    rcv_timeout.tv_sec = GETTER_RECV_TIMEOUT / 1000000;
    rcv_timeout.tv_usec = GETTER_RECV_TIMEOUT % 1000000;
    if (setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &rcv_timeout, sizeof(rcv_timeout)) == -1) {
        ipx_strerror(errno, err_str);
        IPX_CTX_WARNING(data->ctx, "Listener: Failed to specify receiving timeout of a socket: %s",
            err_str);
    }

    // Get the description of the remove address
    struct sockaddr_storage src_addr;
    socklen_t src_addrlen = sizeof(src_addr);
    if (getpeername(sd, (struct sockaddr *) &src_addr, &src_addrlen) == -1) {
        ipx_strerror(errno, err_str);
        IPX_CTX_ERROR(data->ctx, "Listener: Failed to get the remote IP address. getpeername() "
            "failed: %s", err_str);
        return IPX_ERR_DENIED;
    }

    // Get the description of the local address
    struct sockaddr_storage dst_addr;
    socklen_t dst_addrlen = sizeof(dst_addr);
    if (getsockname(sd, (struct sockaddr *) &dst_addr, &dst_addrlen) == -1) {
        ipx_strerror(errno, err_str);
        IPX_CTX_ERROR(data->ctx, "Listener: Failed to get the local IP address. getsockname() "
            "failed: %s", err_str);
        return IPX_ERR_DENIED;
    }

    if (src_addr.ss_family != dst_addr.ss_family) {
        IPX_CTX_ERROR(data->ctx, "Listener: New connection with different family of local and "
            "remote IP addresses rejected!", '\0');
        return IPX_ERR_DENIED;
    }

    // Create a description of the new session
    struct ipx_session_net net;
    memset(&net, 0, sizeof(net));

    net.l3_proto = src_addr.ss_family;
    if (net.l3_proto == AF_INET) { // IPv4
        const struct sockaddr_in *src_v4 = (struct sockaddr_in *) &src_addr;
        const struct sockaddr_in *dst_v4 = (struct sockaddr_in *) &dst_addr;
        net.port_src = ntohs(src_v4->sin_port);
        net.port_dst = ntohs(dst_v4->sin_port);
        net.addr_src.ipv4 = src_v4->sin_addr;
        net.addr_dst.ipv4 = dst_v4->sin_addr;
    } else if (net.l3_proto == AF_INET6) { // IPv6
        const struct sockaddr_in6 *src_v6 = (struct sockaddr_in6 *) &src_addr;
        const struct sockaddr_in6 *dst_v6 = (struct sockaddr_in6 *) &dst_addr;
        net.port_src = ntohs(src_v6->sin6_port);
        net.port_dst = ntohs(dst_v6->sin6_port);
        if (IN6_IS_ADDR_V4MAPPED(&src_v6->sin6_addr) && IN6_IS_ADDR_V4MAPPED(&dst_v6->sin6_addr)) {
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
        IPX_CTX_ERROR(data->ctx, "Listener: New connection with an unsupported IP address family "
            "rejected (family ID: %u)!", (unsigned) src_addr.ss_family);
        return IPX_ERR_DENIED;
    }

    char src_addr_str[INET6_ADDRSTRLEN] = {0};
    inet_ntop(net.l3_proto, &net.addr_src, src_addr_str, INET6_ADDRSTRLEN);

    struct ipx_session *session = ipx_session_new_tcp(&net);
    if (!session || active_session_add(data, sd, session) != IPX_OK) {
        // Failed to add the session
        IPX_CTX_ERROR(data->ctx, "Listener: Failed to add internal information about a new "
            "Transport Session from '%s'! Connection rejected.", src_addr_str);
        if (session != NULL) {
            ipx_session_destroy(session);
        }
        return IPX_ERR_DENIED;
    }

    IPX_CTX_INFO(data->ctx, "New exporter connected from '%s'.", src_addr_str);
    return IPX_OK;
}

/**
 * \brief Thread function of connection acceptor
 *
 * The thread accept new connections and inserts them into the list of active connections
 * and registers them on the epoll instance of active connections.
 * \param[in] cfg Instance data
 * \return Never returns. Must be violently terminated.
 */
static void *
listener_thread(void *cfg)
{
    struct tcp_data *data = (struct tcp_data *) cfg;

    // Always process only one request
    static const int ev_size = 1;
    struct epoll_event ev[ev_size];
    const char *err_str;

    while (1) {
        // Wait for an event (indefinitely)
        int ret = epoll_wait(data->listen.epoll_fd, ev, ev_size, -1);
        if (ret == -1) {
            if (errno == EINTR) {
                // Interrupted!
                continue;
            }

            ipx_strerror(errno, err_str);
            IPX_CTX_ERROR(data->ctx, "Listener: Cannot accept new connections. "
                "epoll_wait() failed: %s", err_str);
            break;
        }

        if (ret != 1) {
            continue;
        }

        // Accept the connection
        int new_sd = accept(ev->data.fd, NULL, NULL);
        if (new_sd == INVALID_FD) {
            ipx_strerror(errno, err_str);
            IPX_CTX_ERROR(data->ctx, "Listener: Failed to accept a new connection: %s", err_str);
            continue;
        }

        // We don't want to be cancel now ----------------------------------------------------------
        int old_state;
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state);

        if (listener_add_connection(data, new_sd) != IPX_OK) {
            // Failed
            close(new_sd);
        }

        // Enable cancelability again
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
    }

    pthread_exit(0);
}

/**
 * \brief Start a thread of the listener
 * \param[in] ctx  Instance context
 * \param[in] data Instance data
 * \return #IPX_OK on success and the thread is running
 * \return #IPX_ERR_DENIED on failure and the thread is not running
 */
static int
listener_start(ipx_ctx_t *ctx, struct tcp_data *data)
{
    int rc = pthread_create(&data->listen.thread, NULL, &listener_thread, data);
    if (rc != 0) {
        const char *err_str;
        ipx_strerror(rc, err_str);
        IPX_CTX_ERROR(ctx, "Failed to create listening thread! (%s)", err_str);
        return IPX_ERR_DENIED;
    }

    return IPX_OK;
}

/**
 * \brief Stop a thread of the listener
 * \param[in] ctx  Instance context
 * \param[in] data Instance data
 */
static void
listener_stop(ipx_ctx_t *ctx, struct tcp_data *data)
{
    const char *err_str;

    int rc = pthread_cancel(data->listen.thread);
    if (rc != 0) {
        ipx_strerror(rc, err_str);
        IPX_CTX_ERROR(ctx, "Failed to cancel listening thread! (%s)", err_str);
        // No return here, because the thread could terminate itself in case of an error
    }

    rc = pthread_join(data->listen.thread, NULL);
    if (rc != 0) {
        ipx_strerror(rc, err_str);
        IPX_CTX_ERROR(ctx, "Failed to cancel listening thread! (%s)", err_str);
        return;
    }

    IPX_CTX_DEBUG(ctx, "Listener thread joined!", '\0');
}

/**
 * \brief Create a new socket, bind it to a local address and enable listening for connections
 *
 * Local address and port is taken from \p addr description.
 * \param[in] ctx      Instance contest
 * \param[in] addr     Local IPv4/IPv6 address and port of the socket(sockaddr_in6 or sockaddr_in)
 * \param[in] addrlen  Size of the address
 * \param[in] ipv6only Accept only IPv6 addresses (only for AF_INET6 and the wildcard address)
 * \return On failure returns #INVALID_FD. Otherwise returns valid socket descriptor.
 */
static int
server_bind_address(ipx_ctx_t *ctx, const struct sockaddr *addr, socklen_t addrlen,
    bool ipv6only)
{
    sa_family_t family = addr->sa_family;
    assert(family == AF_INET || family == AF_INET6);
    int on = 1, off = 0;
    const char *err_str;

    // Create a socket
    int sd = socket(family, SOCK_STREAM, 0);
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
            IPX_CTX_WARNING(ctx, "Cannot turn on socket option IPV6_V6ONLY. Plugin may accept "
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

    // Bind
    if (bind(sd, addr, addrlen) == -1) {
        ipx_strerror(errno, err_str);
        IPX_CTX_ERROR(ctx, "Cannot bind to a socket (local IP: %s, port %" PRIu16 "): %s",
            addr_str, port, err_str);
        close(sd);
        return INVALID_FD;
    }

    // Start listening on the port
    if (listen(sd, LISTEN_BACKLOG) == -1) {
        ipx_strerror(errno, err_str);
        IPX_CTX_ERROR(ctx, "Cannot listen on a socket (local IP: %s, port %" PRIu16 "): %s",
            addr_str, port, err_str);
        close(sd);
        return INVALID_FD;
    }

    IPX_CTX_INFO(ctx, "Listening on %s (port %" PRIu16 ")", addr_str, port);
    return sd;
}

/**
 * \brief Initialize the listener structure of the instance
 *
 * If the no local IP address is defined, the function create one wildcard socket to listen on
 * all local interfaces. Otherwise for each specified address create a separated socket.
 * All sockets are stored into the array (structure listen) of the instance configuration and
 * epoll with added socket is created.
 *
 * \param[in] ctx      Instance context
 * \param[in] instance Instance data
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED on failure (typically failed to bind to a port)
 */
static int
listener_init(ipx_ctx_t *ctx, struct tcp_data *instance)
{
    // Create a poll and new array of binded sockets
    const char *err_str;
    const size_t socket_cnt = instance->config->local_addrs.cnt;
    int *sockets = malloc(sizeof(*sockets) * ((socket_cnt == 0) ? 1 : socket_cnt));
    if (!sockets) {
        IPX_CTX_ERROR(ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        return IPX_ERR_DENIED;
    }

    int epoll_fd = epoll_create(1);
    if (epoll_fd == INVALID_FD) {
        ipx_strerror(errno, err_str);
        IPX_CTX_ERROR(ctx, "epoll() failed: %s", err_str);
        free(sockets);
        return IPX_ERR_DENIED;
    }

    if (socket_cnt == 0) {
        // Wildcard (i.e. bind to all IPv4 and IPv6 addresses)
        struct sockaddr_in6 addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(instance->config->local_port);
        addr.sin6_addr = in6addr_any;

        int sd = server_bind_address(ctx, (struct sockaddr *) &addr, sizeof(addr), false);
        if (sd == INVALID_FD) {
            free(sockets);
            close(epoll_fd);
            return IPX_ERR_DENIED;
        }

        // Add the socket to the poll
        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN;
        ev.data.fd = sd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sd, &ev) == -1) {
            ipx_strerror(errno, err_str);
            IPX_CTX_ERROR(ctx, "Failed to add a socket to epoll: %s", err_str);
            close(sd);
            close(epoll_fd);
            free(sockets);
            return IPX_ERR_DENIED;
        }

        sockets[0] = sd;
        instance->listen.sockets = sockets;
        instance->listen.cnt = 1; // Only this one
        instance->listen.epoll_fd = epoll_fd;
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

        const struct tcp_ipaddr_rec *rec = &instance->config->local_addrs.addrs[idx];
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

        int sd = server_bind_address(ctx, (struct sockaddr *) &addr_helper, addrlen, ipv6only);
        if (sd == INVALID_FD) {
            // Failed
            break;
        }

        // Add the socket to the poll
        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN;
        ev.data.fd = sd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sd, &ev) == -1) {
            ipx_strerror(errno, err_str);
            IPX_CTX_ERROR(ctx, "Failed to add a socket to epoll: %s", err_str);
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

        close(epoll_fd);
        free(sockets);
        return IPX_ERR_DENIED;
    }

    instance->listen.sockets = sockets;
    instance->listen.cnt = socket_cnt;
    instance->listen.epoll_fd = epoll_fd;
    return IPX_OK;
}

/**
 * \brief Destroy the listener structure of the instance
 *
 * All acceptor sockets are closed and unregistered from epoll. Epoll is also destroyed.
 * \warning Make sure that acceptor thread is not running!
 * \param[in] instance Instance data
 */
static void
listener_destroy(struct tcp_data *instance)
{
    for (size_t i = 0; i < instance->listen.cnt; ++i) {
        // Remove from epoll and close
        int sd = instance->listen.sockets[i];
        epoll_ctl(instance->listen.epoll_fd, EPOLL_CTL_DEL, sd, NULL);
        close(sd);
    }

    close(instance->listen.epoll_fd);
    free(instance->listen.sockets);
    instance->listen.sockets = NULL;
    instance->listen.cnt = 0;
}

/**
 * \brief Initialize the active structure of the instance
 *
 * Initialize empty epoll and lock of the active connections.
 * \param[in] ctx      Instance context
 * \param[in] instance Instance data
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED on failure
 */
static int
active_init(ipx_ctx_t *ctx, struct tcp_data *instance)
{
    const char *err_str;

    // Initialize empty epoll
    int epoll_fd = epoll_create(1);
    if (epoll_fd == INVALID_FD) {
        ipx_strerror(errno, err_str);
        IPX_CTX_ERROR(ctx, "epoll() failed: %s", err_str);
        return IPX_ERR_DENIED;
    }

    int rc = pthread_mutex_init(&instance->active.lock, NULL);
    if (rc != 0) {
        ipx_strerror(rc, err_str);
        IPX_CTX_ERROR(ctx, "pthread_mutex_init() failed: %s", err_str);
        close(epoll_fd);
        return IPX_ERR_DENIED;
    }

    instance->active.cnt = 0;
    instance->active.pairs = NULL;
    instance->active.epoll_fd = epoll_fd;
    return IPX_OK;
}

/**
 * \brief Destroy the active structure of the instance
 *
 * Send Session messages that all connection were closed, destroy the lock and epoll.
 * \warning Make sure that acceptor thread is not running!
 * \param[in] ctx      Instance context
 * \param[in] instance Instance data
 */
static void
active_destroy(ipx_ctx_t *ctx, struct tcp_data *instance)
{
    (void) ctx;

    // Close all active connections
    while (instance->active.cnt > 0) {
        const struct ipx_session *session = instance->active.pairs[0]->session; // The first one
        active_session_remove_by_session(instance, session);
    }

    pthread_mutex_destroy(&instance->active.lock);
    close(instance->active.epoll_fd);
    free(instance->active.pairs);
    instance->active.cnt = 0;
}

/**
 * \brief Get an IPFIX message from a socket and pass it
 *
 * \param[in] ctx  Instance data (necessary for passing messages)
 * \param[in] pair Connection pair (socket descriptor and session) to receive from
 * \return #IPX_OK on success
 * \return #IPX_ERR_EOF if the socket is closed
 * \return #IPX_ERR_FORMAT if the message (or stream) is malformed and the connection MUST be closed
 * \return #IPX_ERR_NOMEM on a memory allocation error and the connection MUST be closed
 */
static int
socket_process(ipx_ctx_t *ctx, struct tcp_pair *pair)
{
    const char *err_str;
    struct fds_ipfix_msg_hdr hdr;
    static_assert(sizeof(hdr) == FDS_IPFIX_MSG_HDR_LEN, "Invalid size of IPFIX Message header");

    // Get the message header (do not move pointer)
    ssize_t len = recv(pair->fd, &hdr, FDS_IPFIX_MSG_HDR_LEN, MSG_WAITALL | MSG_PEEK);
    if (len == 0) {
        // Connection terminated
        IPX_CTX_INFO(ctx, "Connection from '%s' closed.", pair->session->ident);
        return IPX_ERR_EOF;
    }

    if (len == -1 || len < FDS_IPFIX_MSG_HDR_LEN) {
        // Failed to read header -> close
        int error_code = (len == -1) ? errno : EINTR;
        ipx_strerror(error_code, err_str);
        IPX_CTX_WARNING(ctx, "Connection from '%s' closed due to failure to receive "
            "an IPFIX Message header: %s", pair->session->ident, err_str);
        return IPX_ERR_FORMAT;
    }
    assert(len == FDS_IPFIX_MSG_HDR_LEN);

    // Check the header (version, size)
    uint16_t msg_version = ntohs(hdr.version);
    uint16_t msg_size = ntohs(hdr.length);
    uint32_t msg_odid = ntohl(hdr.odid);

    if (msg_version != FDS_IPFIX_VERSION || msg_size < FDS_IPFIX_MSG_HDR_LEN) {
        // Unsupported header version
        IPX_CTX_WARNING(ctx, "Connection from '%s' closed due to the unsupported version of "
            "IPFIX/NetFlow.", pair->session->ident);
        return IPX_ERR_FORMAT;
    }

    // Read the whole message
    uint8_t *buffer = malloc(msg_size * sizeof(uint8_t));
    if (!buffer) {
        IPX_CTX_ERROR(ctx, "Connection from '%s' closed due to memory allocation failure! (%s:%d).",
            pair->session->ident, __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }

    len = recv(pair->fd, buffer, msg_size, MSG_WAITALL);
    if (len != msg_size) {
        int error_code = (len == -1) ? errno : ETIMEDOUT;
        ipx_strerror(error_code, err_str);
        IPX_CTX_ERROR(ctx, "Connection from '%s' closed due to failure while reading from "
            "its socket: %s.", pair->session->ident, err_str);
        free(buffer);
        return IPX_ERR_FORMAT;
    }

    if (pair->new_connection) {
        // Send information about the new Transport Session
        pair->new_connection = false;
        ipx_msg_session_t *msg = ipx_msg_session_create(pair->session, IPX_MSG_SESSION_OPEN);
        if (!msg) {
            IPX_CTX_ERROR(ctx, "Connection from '%s' closed due to memory allocation "
                "failure! (%s:%d).", pair->session->ident, __FILE__, __LINE__);
            free(buffer);
            return IPX_ERR_NOMEM;
        }

        ipx_ctx_msg_pass(ctx, ipx_msg_session2base(msg));
    }

    // Create a message wrapper and pass the message
    struct ipx_msg_ctx msg_ctx;
    msg_ctx.session = pair->session;
    msg_ctx.odid = msg_odid;
    msg_ctx.stream = 0; // Streams are not supported over TCP

    ipx_msg_ipfix_t *msg = ipx_msg_ipfix_create(ctx, &msg_ctx, buffer, msg_size);
    if (!msg) {
        IPX_CTX_ERROR(ctx, "Connection from '%s' closed due to memory allocation "
            "failure! (%s:%d).", pair->session->ident, __FILE__, __LINE__);
        free(buffer);
        return IPX_ERR_NOMEM;
    }

    ipx_ctx_msg_pass(ctx, ipx_msg_ipfix2base(msg));
    return IPX_OK;
}

// -------------------------------------------------------------------------------------------------

int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params)
{
    struct tcp_data *data = calloc(1, sizeof(*data));
    if (!data) {
        IPX_CTX_ERROR(ctx, "Memory allocation failed! (%s:%d)", __FILE__, __LINE__);
        return IPX_ERR_DENIED;
    }
    data->ctx = ctx;

    // Parse configuration
    data->config = config_parse(ctx, params);
    if (!data->config) {
        free(data);
        return IPX_ERR_DENIED;
    }

    // Initialize structures of the listener (and bind to the local addresses)
    if (listener_init(ctx, data) != IPX_OK) {
        config_destroy(data->config);
        free(data);
        return IPX_ERR_DENIED;
    }

    // Initialize structures of active connections
    if (active_init(ctx, data) != IPX_OK) {
        listener_destroy(data);
        config_destroy(data->config);
        free(data);
        return IPX_ERR_DENIED;
    }

    // Start the acceptor thread
    if (listener_start(ctx, data) != IPX_OK) {
        active_destroy(ctx, data);
        listener_destroy(data);
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
    struct tcp_data *data = (struct tcp_data *) cfg;

    // Stop the acceptor and destroy its structure
    listener_stop(ctx, data);
    listener_destroy(data);

    // Close all Transport Session (this generates Session messages per each active Session)
    active_destroy(ctx, data);

    // Final cleanup
    config_destroy(data->config);
    free(data);
}

int
ipx_plugin_get(ipx_ctx_t *ctx, void *cfg)
{
    struct tcp_data *data = (struct tcp_data *) cfg;
    const char *err_str;

    // Process messages from up to 16 sockets
    struct epoll_event ev[GETTER_MAX_EVENTS];
    int ev_valid = epoll_wait(data->active.epoll_fd, ev, GETTER_MAX_EVENTS, GETTER_TIMEOUT);
    if (ev_valid == -1) {
        // Failed
        int error_code = errno;
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
        struct tcp_pair *pair = (struct tcp_pair *) ev[i].data.ptr;
        if (socket_process(ctx, pair) == IPX_OK) {
            // Success
            continue;
        }

        // The Transport Session is broken -> close it
        active_session_remove_by_session(data, pair->session);
        // From this point the pair doesn't exists, and the session is probably destroyed
    }

    return IPX_OK;
}

void
ipx_plugin_session_close(ipx_ctx_t *ctx, void *cfg, const struct ipx_session *session)
{
    struct tcp_data *data = (struct tcp_data *) cfg;
    // Do NOT dereference the session pointer because it can be already freed!

    if (active_session_remove_by_session(data, session) != IPX_OK) {
        /* The session is not present in the buffer, probably because we already removed it
         * before the parser send the request to close the session
         */
        IPX_CTX_WARNING(ctx, "Received a request to close a unknown Transport Session!", '\0');
        return;
    }
}