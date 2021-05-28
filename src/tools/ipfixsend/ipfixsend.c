/**
 * \file ipfixsend.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Tool that sends ipfix packets
 *
 * Copyright (C) 2016-2018 CESNET, z.s.p.o.
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/ioctl.h>
#ifdef __linux__
#include <linux/sockios.h>
#endif
#include <time.h>

#include "siso.h"
#include "reader.h"
#include "sender.h"

/** Default destination IP                 */
#define DEFAULT_IP "127.0.0.1"
/** Default destination port               */
#define DEFAULT_PORT "4739"
/** Default transport protocol             */
#define DEFAULT_TYPE "UDP"
/** By default, send data in infinite loop */
#define INFINITY_LOOPS (-1)
/**
 * Timeout for waiting until all queued messages have been sent before close()
 * (in nanoseconds)
 */
#define FLUSHER_TIME 100000000LL

/** Global stop signal                     */
static volatile sig_atomic_t stop = 0;

/** \brief Print usage                     */
void usage()
{
    printf("\n");
    printf("Usage: ipfixsend [options]\n");
    printf("  -h         Show this help\n");
    printf("  -i path    IPFIX input file\n");
    printf("  -d ip      Destination IP address (default: %s)\n", DEFAULT_IP);
    printf("  -p port    Destination port number (default: %s)\n", DEFAULT_PORT);
    printf("  -t type    Connection type (UDP or TCP) (default: UDP)\n");
    printf("  -c         Precache input file (for performance tests)\n");
    printf("  -n num     How many times the file should be sent (default: infinity)\n");
    printf("  -s speed   Maximum data sending speed/s\n");
    printf("             Supported suffixes: B (default), K, M, G\n");
    printf("  -S packets Speed limit in packets/s\n");
    printf("  -R num     Real-time sending\n");
    printf("             Allow speed-up sending 'num' times (realtime: 1.0)\n");
    printf("  -O num     Rewrite Observation Domain ID (ODID)\n");
    printf("\n");
}

/**
 * \brief Termination signal handler
 * \param[in] signal Signal to process
 */
void handler(int signal)
{
    (void) signal; // skip compiler warning
    sender_stop();
    stop = 1;
}

/**
 * \brief Main function
 * \param[in] argc Number of arguments
 * \param[in] argv Array of arguments
 */
int main(int argc, char** argv)
{
    char   *ip =   DEFAULT_IP;
    char   *port = DEFAULT_PORT;
    char   *type = DEFAULT_TYPE;

    char   *input = NULL;
    char   *speed = NULL;
    int     loops =   INFINITY_LOOPS;
    int     packets_s = 0;
    double  realtime_s = 0.0;
    bool    precache = false;

    bool    odid_rewrite = false;
    long    odid_new;

    if (argc == 1) {
        usage();
        return 0;
    }

    // Parse parameters
    int c;
    while ((c = getopt(argc, argv, "hci:d:p:t:n:s:S:R:O:")) != -1) {
        switch (c) {
        case 'h':
            usage();
            return 0;
        case 'i':
            input = optarg;
            break;
        case 'd':
            ip = optarg;
            break;
        case 'p':
            port = optarg;
            break;
        case 't':
            type = optarg;
            break;
        case 'c':
            precache = true;
            break;
        case 'n':
            loops = atoi(optarg);
            break;
        case 's':
            speed = optarg;
            break;
        case 'S':
            packets_s = atoi(optarg);
            break;
        case 'R':
            realtime_s = atof(optarg);
            break;
        case 'O':
            odid_rewrite = true;
            odid_new = atol(optarg);
            break;
        default:
            fprintf(stderr, "Unknown option.\n");
            return 1;
        }
    }

    // Parameters check
    if (loops < 0 && loops != INFINITY_LOOPS) {
        fprintf(stderr, "Invalid value of replay loops\n");
        return 1;
    }

    if (packets_s < 0) {
        fprintf(stderr, "Invalid value of the packet speed limitation.\n");
        return 1;
    }

    if (realtime_s < 0.0) {
        fprintf(stderr, "Invalid value of the real-time sending.\n");
        return 1;
    }

    if ((speed != NULL || packets_s != 0) && realtime_s > 0) {
        fprintf(stderr, "Combination of real-time sending and speed limitation "
            "is not permitted.\n");
        return 1;
    }

    if (odid_rewrite && (odid_new < 0 || odid_new > UINT32_MAX)) {
        fprintf(stderr, "Invalid ODID value. Must be in range (0 .. %" PRIu32 ")\n",
            UINT32_MAX);
        return 1;
    }

    // Check whether everything is set
    if (!input) {
        fprintf(stderr, "Input file must be set!\n");
        return 1;
    }

    signal(SIGINT, handler);

    // Get collector's address
    sisoconf *sender = siso_create();
    if (!sender) {
        fprintf(stderr, "Memory allocation error\n");
        return 1;
    }

    // Prepare an input file
    reader_t *reader = reader_create(input, precache);
    if (!reader) {
        siso_destroy(sender);
        return 1;
    }

    if (odid_rewrite) {
        reader_odid_rewrite(reader, (uint32_t) odid_new);
    }

    if (loops != 1) {
        reader_header_autoupdate(reader, true);
    }

    // Create connection
    int ret = siso_create_connection(sender, ip, port, type);
    if (ret != SISO_OK) {
        fprintf(stderr, "Network error: %s\n", siso_get_last_err(sender));
        siso_destroy(sender);
        reader_destroy(reader);
        return 1;
    }

    // Set max. speed
    if (speed) {
        siso_set_speed_str(sender, speed);
    }

    // Send packets
    int i;
    for (i = 0; !stop && (loops == INFINITY_LOOPS || i < loops); ++i) {
        reader_rewind(reader);
        if (realtime_s > 0.0) {
            // Real-time sending
            ret = send_packets_realtime(sender, reader, realtime_s);
        } else {
            // Speed limitation sending
            ret = send_packets_limit(sender, reader, packets_s);
        }

        if (ret != 0) {
            // Error
            break;
        }
    }

    // Free resources
    reader_destroy(reader);

    // Make sure that all packets are delivered before socket closes
    int socket_fd = siso_get_socket(sender);
    int not_sent = 0;
#ifndef SIOCOUTQ
#define SIOCOUTQ TIOCOUTQ
#endif
    while (!stop && ioctl(socket_fd, SIOCOUTQ, &not_sent) != -1) {
        if (not_sent <= 0) {
            break;
        }

        // Wait
        struct timespec sleep_time = {0, FLUSHER_TIME};
        nanosleep(&sleep_time, NULL);
    }

    siso_destroy(sender);
    return 0;
}
