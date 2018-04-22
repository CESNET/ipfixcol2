/**
 * \file ipfixsend/sender.h
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
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

#ifndef SENDER_H
#define	SENDER_H

#include <netdb.h>
#include "siso.h"
#include "reader.h"

/**
 * \brief Send all packets from array with speed limitation
 *
 * \param[in] sender    sisoconf object
 * \param[in] reader    Input file
 * \param[in] packets_s packets/s limit
 * \return On success returns 0. Otherwise returns nonzero value.
 */
int send_packets_limit(sisoconf *sender, reader_t *reader, int packets_s);

/**
 * \brief Send all packets from array with real-time simulation
 *
 * \param[in] sender sisoconf object
 * \param[in] reader Input file
 * \param[in] speed  Speed-up compared to real-time (multiples)
 * \return On success returns 0. Otherwise returns nonzero value.
 */
int send_packets_realtime(sisoconf *sender, reader_t *reader, double speed);

/**
 * \brief Stop sending data
 */
void sender_stop();

#endif	/* SENDER_H */

