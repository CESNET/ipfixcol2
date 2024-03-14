/**
 * \file siso.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Simple socket library for sending data over the network
 *
 * Copyright (C) 2015-2018 CESNET, z.s.p.o.
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
 * This software is provided ``as is, and any express or implied
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

#include "siso.h"

#include <stdbool.h>
#include <stdlib.h>

#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <sys/time.h>
#include <time.h>

#include <fcntl.h>
#include <stdio.h>
#include <strings.h>

/**
 * \brief Check that a pointer is not null. Otherwise returns #SISO_ERR
 * \param[in] ptr Pointer to check
 */
#define CHECK_PTR(ptr) if (!(ptr)) return (SISO_ERR)
/**
 * \brief Check that a status code is #SISO_OK. Otherwise return #SISO_ERR.
 * \param[in] retval Status code to check
 */
#define CHECK_RETVAL(retval) if ((retval) != SISO_OK) return (SISO_ERR)
/**
 * \brief Get the lass error message
 */
#define PERROR_LAST strerror(errno)
/** Maximum message size that can be send over UDP                          */
#define SISO_UDP_MAX 65000
/**
 * \brief Get minimum from 2 values
 * \param[in] frst First value
 * \param[in] scnd Second value
 * \return Minimum
 */
#define SISO_MIN(frst, scnd) ((frst) > (scnd) ? (scnd) : (frst))

/** Accepted connection types                */
enum siso_conn_type {
   SC_UDP,
   SC_TCP,
   SC_SCTP,
   SC_UNKNOWN,
};

/** Message indexes                           */
enum SISO_ERRORS {
    SISO_ERR_OK,
    SISO_ERR_TYPE,
    SISO_ERR_CONNECT,
    SISO_ERR_CONFIG
};

/** Error message  */
static const char *siso_messages[] = {
    [SISO_ERR_OK]      = "Everything OK",
    [SISO_ERR_TYPE]    = "Unknown connection type",
    [SISO_ERR_CONNECT] = "Unable to create a new socket and connect to a destination.",
    [SISO_ERR_CONFIG]  = "Configuration information is missing."
};

/** Supported connection types      */
static const char *siso_sc_types[] = {
    [SC_UDP]  = "UDP",
    [SC_TCP]  = "TCP",
    [SC_SCTP] = "SCTP"
};

/**
 * \brief Main sisolib structure
 */
struct sisoconf_s {
    const char *last_error;     /**< last error message */
    enum siso_conn_type type;   /**< UDP/TCP/SCTP */
    struct addrinfo *servinfo;  /**< server information */
    int sockfd;                 /**< socket descriptor */
    uint64_t max_speed;         /**< max sending speed */
    uint64_t act_speed;         /**< actual speed */
    struct timeval begin, end;  /**< start/end time for limited transfers */
};

/**
 * \brief Constructor
 */
sisoconf *siso_create()
{
    /* allocate memory */
    sisoconf *conf = (sisoconf *) calloc(1, sizeof(sisoconf));
    if (!conf) {
        return NULL;
    }

    conf->sockfd = -1;
    /* Set default message */
    conf->last_error = siso_messages[SISO_ERR_OK];
    return conf;
}

/**
 * \brief Destructor
 */
void siso_destroy(sisoconf *conf)
{
    /* Close socket and free resources */
    siso_close_connection(conf);

    if (conf->servinfo != NULL) {
        freeaddrinfo(conf->servinfo);
    }

    free(conf);
}

/**
 * \brief Converts connection type from string to enum
 */
enum siso_conn_type siso_decode_type(sisoconf *conf, const char *type)
{
    int i;
    for (i = 0; i < (int) SC_UNKNOWN; ++i) {
        if (!strcasecmp(type, siso_sc_types[i])) {
            conf->type = (enum siso_conn_type) i;
            return SISO_OK;
        }
    }

    // Unknown connection type
    conf->last_error = siso_messages[SISO_ERR_TYPE];
    return SISO_ERR;
}

// Getters
inline int siso_get_socket(sisoconf *conf)		{ return conf->sockfd; }
inline int siso_get_conn_type(sisoconf *conf)   { return conf->type; }
inline uint64_t siso_get_speed(sisoconf *conf)  { return conf->max_speed; }
inline const char *siso_get_last_err(sisoconf *conf){ return conf->last_error; }

// Check if a destination is connected
int siso_is_connected(const sisoconf *conf)
{
    return (conf->sockfd == -1) ? 0 : 1;
}

/**
 * \brief Set unlimited speed
 */
void siso_unlimit_speed(sisoconf* conf)
{
    conf->max_speed = 0;
}

/**
 * \brief Set speed limit
 */
void siso_set_speed(sisoconf* conf, uint32_t limit, enum siso_units units)
{
    conf->max_speed = limit;

    switch (units) {
    case SU_KBYTE:
        conf->max_speed *= 1024;
        break;
    case SU_MBYTE:
        conf->max_speed *= 1024 * 1024;
        break;
    case SU_GBYTE:
        conf->max_speed *= 1024 * 1024 * 1024;
        break;
    default:
        break;
    }
}

/**
 * \brief Set speed limit
 */
void siso_set_speed_str(sisoconf* conf, const char* limit)
{
    conf->max_speed = strtoul(limit, NULL, 10);
    char last = limit[strlen(limit) - 1];

    switch (last) {
    case 'k': case 'K':
        conf->max_speed *= 1024;
        break;
    case 'm': case 'M':
        conf->max_speed *= 1024 * 1024;
        break;
    case 'g': case 'G':
        conf->max_speed *= 1024 * 1024 * 1024;
        break;
    default:
        break;
    }
}

/**
 * \brief Get server information
 */
int siso_getaddrinfo(sisoconf *conf, const char *ip, const char *port)
{
    struct addrinfo hints;

    // Get server address
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;

    switch (conf->type) {
    case SC_UDP:
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
        break;
    case SC_TCP:
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        break;
    case SC_SCTP:
        hints.ai_socktype = SOCK_STREAM; // SOCK_SEQPACKET;
        hints.ai_protocol = IPPROTO_SCTP;
        break;
    default:
        break;
    }

    if (conf->servinfo != NULL) {
        freeaddrinfo(conf->servinfo);
        conf->servinfo = NULL;
    }

    if (getaddrinfo(ip, port, &hints, &(conf->servinfo)) != 0) {
        conf->last_error = PERROR_LAST;
        return SISO_ERR;
    }

    return SISO_OK;
}

/**
 * \brief Create new socket
 */
int siso_create_socket(sisoconf *conf)
{
    struct addrinfo *iter;
    int new_fd = -1;

    for (iter = conf->servinfo; iter != NULL; iter = iter->ai_next) {
        // Create a socket
        new_fd = socket(iter->ai_family, iter->ai_socktype, iter->ai_protocol);
        if (new_fd == -1) {
            continue;
        }

        if (connect(new_fd, iter->ai_addr, iter->ai_addrlen) != -1) {
            // Success
            break;
        }

        close(new_fd);
    }

    if (iter == NULL) {
        conf->last_error = siso_messages[SISO_ERR_CONNECT];
        return SISO_ERR;
    }

    conf->sockfd = new_fd;
    return SISO_OK;
}

/**
 * \brief Create new connection
 */
int siso_create_connection(sisoconf* conf, const char* ip, const char *port, const char *type)
{
    CHECK_PTR(conf);

    // Close previous connection
    if (conf->sockfd > 0) {
        siso_close_connection(conf);
    }

    // Set connection type
    int ret = siso_decode_type(conf, type);
    CHECK_RETVAL(ret);

    // Get server info
    ret = siso_getaddrinfo(conf, ip, port);
    CHECK_RETVAL(ret);

    // Create socket
    ret = siso_create_socket(conf);
    CHECK_RETVAL(ret);

    return SISO_OK;
}

/**
 * \brief Close actual connection
 */
void siso_close_connection(sisoconf *conf)
{
    if (conf && conf->sockfd > 0) {
        close(conf->sockfd);
        conf->sockfd = -1;
    }
}

// Reconnect to the destination
int siso_reconnect(sisoconf *conf)
{
    if (conf->servinfo == NULL) {
        conf->last_error = siso_messages[SISO_ERR_CONFIG];
        return SISO_ERR;
    }

    siso_close_connection(conf);
    return siso_create_socket(conf);
}

/**
 * \brief Send data
 */
int siso_send(sisoconf *conf, const char *data, ssize_t length)
{
    CHECK_PTR(conf);

    // Pointer to data to be sent
    const char *ptr = data;

    // data sent per cycle
    ssize_t sent_now = 0;

    // Size of remaining data
    ssize_t todo = length;

    while (todo > 0) {
        // Send data
        switch (conf->type) {
        case SC_UDP:
            sent_now = send(conf->sockfd, ptr, SISO_MIN(todo, SISO_UDP_MAX),
                MSG_NOSIGNAL);
            break;
        case SC_TCP:
        case SC_SCTP:
            sent_now = send(conf->sockfd, ptr, todo, MSG_NOSIGNAL);
            break;
        default:
            break;
        }

        // Check for errors
        if (sent_now == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                // Connection broken, close...
                conf->last_error = PERROR_LAST;
                siso_close_connection(conf);
                return SISO_ERR;
            }

            // Probably a signal occurred. Try again.
            continue;
        }

        // Skip sent data
        ptr  += sent_now;
        todo -= sent_now;

        // check speed limit
        conf->act_speed += sent_now;
        if (conf->max_speed && conf->act_speed >= conf->max_speed) {
            gettimeofday(&(conf->end), NULL);

            // Should sleep?
            double elapsed = conf->end.tv_usec - conf->begin.tv_usec;
            if (elapsed < 1000000.0) {
                usleep(1000000.0 - elapsed);
                gettimeofday(&(conf->end), NULL);
            }

            // reinit values
            conf->begin = conf->end;
            conf->act_speed = 0;
        }

    }

    return SISO_OK;
}
