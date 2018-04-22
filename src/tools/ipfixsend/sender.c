/**
 * \file ipfixsend/sender.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \author Lukas Hutak <hutak@cesnet.cz>
 * \brief Functions for parsing IP, connecting to collector and sending packets
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <sys/time.h>
#include <signal.h>

#include "sender.h"
#include "reader.h"
#include "siso.h"

/** 1 second in microseconds     */
#define MICRO_SEC 1000000L
/** 1 second in nanoseconds      */
#define NANO_SEC 1000000000L
/** Global termination flag      */
static volatile sig_atomic_t stop_sending = 0;

/** \brief Interrupt sending     */
void sender_stop()
{
    stop_sending = 1;
}

/**
 * \brief Send a packet
 * \param[in] sender SISO instance
 * \param[in] packet Packet to send
 */
int send_packet(sisoconf *sender, const struct fds_ipfix_msg_hdr *packet)
{
    const size_t size = ntohs(packet->length);
    return siso_send(sender, (const char *) packet, size);
}

/**
 * \brief Calculate time difference
 * \param[in] start First timestamp
 * \param[in] end Second timestamp
 * \return Number of microseconds between timestamps
 */
long timeval_diff(const struct timeval *start, const struct timeval *end)
{
    return (end->tv_sec - start->tv_sec) * MICRO_SEC
        + (end->tv_usec - start->tv_usec);
}

/**
 * \brief Send all packets from array with speed limitation
 */
int send_packets_limit(sisoconf *sender, reader_t *reader, int packets_s)
{
    enum READER_STATUS status;
    struct fds_ipfix_msg_hdr *pkt_data;
    struct timeval begin, end;
    struct timespec sleep_time = {0};

    // These must be static variables - local would be rewritten with each call of send_packets
    int pkts_from_begin = 0;
    double time_per_pkt; // [ms]

    // Absolutely first packet
    gettimeofday(&begin, NULL);
    time_per_pkt = 1000000.0 / packets_s; // [micro seconds]

    while (stop_sending == 0) {
        status = reader_get_next_packet(reader, &pkt_data, NULL);
        if (status == READER_EOF) {
            break;
        } else if (status == READER_ERROR) {
            return 1;
        }

        // send packet
        int ret = send_packet(sender, pkt_data);
        if (ret != SISO_OK) {
            fprintf(stderr, "Network error: %s\n", siso_get_last_err(sender));
            return 1;
        }

        pkts_from_begin++;
        if (packets_s <= 0) {
            // Limit for packets/s is not enabled
            continue;
        }

        // Calculate expected time of sending next packet
        gettimeofday(&end, NULL);
        long elapsed = timeval_diff(&begin, &end);
        if (elapsed < 0) {
            // Should be never negative. Just for sure...
            elapsed = pkts_from_begin * time_per_pkt;
        }

        long next_start = pkts_from_begin * time_per_pkt;
        long diff = next_start - elapsed;

        if (diff >= MICRO_SEC) {
            diff = MICRO_SEC - 1;
        }

        // Sleep
        if (diff > 0) {
            sleep_time.tv_nsec = diff * 1000L;
            nanosleep(&sleep_time, NULL);
        }

        if (pkts_from_begin >= packets_s) {
            // Restart counter
            gettimeofday(&begin, NULL);
            pkts_from_begin = 0;
        }
    };

    return 0;
}

/**
 * \brief Compare IPFIX timestamps numbers (with wraparound support)
 * \param[in] t1 First timestamp
 * \param[in] t2 Second timestamp
 * \return  The function  returns an integer less than, equal to, or greater than zero if the
 *   first number \p t1 is found, respectively, to be less than, to match, or be greater than
 *   the second number.
 */
static inline int
ts_cmp(uint32_t t1, uint32_t t2)
{
    if (t1 == t2) {
        return 0;
    }

    if ((t1 - t2) & 0x80000000) { // test the "sign" bit
        return (-1);
    } else {
        return 1;
    }
}

/**
 * \brief Get the count of packets in a group with the same timestamps
 *
 * The group timestamp is based on the timestamp of the first read packet.
 * The group ends when a packet with later timestamp is detected i.e. the group
 * also includes packets with earlier timestamps.
 *
 * \param[in] reader Packet reader
 * \return On success returns value > 0. On EOF returns 0. Otherwise (errors)
 *   returns a negative number.
 */
int ts_grp_cnt(reader_t *reader)
{
    struct fds_ipfix_msg_hdr *header;
    enum READER_STATUS status;
    uint32_t reference_time;
    int counter;

    if (reader_position_push(reader) != READER_OK) {
        return -1;
    }

    status = reader_get_next_header(reader, &header);
    if (status != READER_OK) {
        return (status == READER_EOF) ? 0 : -1;
    }

    reference_time = ntohl(header->export_time);
    counter = 1;

    // Get next header
    while ((status = reader_get_next_header(reader, &header)) != READER_EOF) {
        if (status != READER_OK) {
            return -1;
        }

        if (ts_cmp(reference_time, ntohl(header->export_time)) < 0) {
            break;
        }

        ++counter;
    };

    if (reader_position_pop(reader) != READER_OK) {
        return -1;
    }

    return counter;
}

/**
 * \brief Send all packets from array with real-time simulation
 */
int send_packets_realtime(sisoconf *sender, reader_t *reader, double speed)
{
    int grp_cnt = 0; // Number of packets in a group with same timestamp
    int grp_id = 0;  // Index of the packet in the group
    uint32_t grp_ts_prev = 0;
    uint32_t grp_ts_now;
    double time_per_pkt = 0.0;
    struct timeval group_ts_start, end;
    struct fds_ipfix_msg_hdr *new_packet = NULL;

    if (reader_position_push(reader) != READER_OK) {
        return 1;
    }

    if (reader_get_next_header(reader, &new_packet) != READER_OK) {
        return 1;
    }

    grp_ts_now = ntohl(new_packet->export_time);
    if (reader_position_pop(reader) != READER_OK) {
        return 1;
    }

    while (stop_sending == 0) {
        if (grp_cnt == grp_id) {
            // New group
            grp_id = 0;
            grp_cnt = ts_grp_cnt(reader);
            if (grp_cnt == 0) {
                // End of file
                break;
            }

            if (grp_cnt < 0) {
                // Error
                return 1;
            }

            // Prepare a new packet
            if (reader_get_next_packet(reader, &new_packet, NULL) != READER_OK) {
                // This can not be EOF -> it must be error
                return 1;
            }

            grp_ts_prev = grp_ts_now;
            grp_ts_now = ntohl(new_packet->export_time);
            time_per_pkt = 1000000.0 / (grp_cnt * speed); // [micro seconds]

            // Sleep between time groups only when difference > 1 second
            if (ts_cmp(grp_ts_now, grp_ts_prev + 1) > 0) {
                double  seconds_diff = (grp_ts_now - grp_ts_prev - 1) / speed;
                struct timespec sleep_time;
                sleep_time.tv_sec = (int) seconds_diff;
                sleep_time.tv_nsec = (seconds_diff - (int) seconds_diff) * NANO_SEC;
                nanosleep(&sleep_time, NULL);
            }

            gettimeofday(&group_ts_start, NULL);
        } else {
            // Prepare a new packet
            if (reader_get_next_packet(reader, &new_packet, NULL) != READER_OK) {
                // This can not be EOF -> it must be error
                return 1;
            }
        }

        // Send the packet
        int ret = send_packet(sender, new_packet);
        if (ret != SISO_OK) {
            fprintf(stderr, "Network error: %s\n", siso_get_last_err(sender));
            return 1;
        }

        ++grp_id;

        // Calculate expected time of sending next packet
        gettimeofday(&end, NULL);
        long elapsed = timeval_diff(&group_ts_start, &end);
        if (elapsed < 0) {
            // Should be never negative. Just for sure...
            elapsed = grp_id * time_per_pkt;
        }

        long next_start = grp_id * time_per_pkt;
        long diff = next_start - elapsed;

        // Sleep between packets in the group
        if (diff > 0) {
            struct timespec sleep_time;
            sleep_time.tv_sec = diff / MICRO_SEC;
            sleep_time.tv_nsec = (diff % MICRO_SEC) * 1000L;
            nanosleep(&sleep_time, NULL);
        }
    }

    return 0;
}
