/**
 * @file
 * @author Adrian Duriska <xdurisa00@stud.fit.vutbr.cz>
 * @brief Periodic message
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IPX_MESSAGE_PERIODIC_H
#define IPX_MESSAGE_PERIODIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ipfixcol2/message.h>

typedef struct ipx_msg_periodic ipx_msg_periodic_t;

/**
 * \brief Cast periodic message to base message
 *
 * \param[in] msg   Pointer to the periodic message
 * \return A pointer to a base message
 */
static inline ipx_msg_t *
ipx_msg_periodic2base(ipx_msg_periodic_t *msg)
{
    return (ipx_msg_t *) msg;
}

/**
 * \brief Get sequence number of periodic message
 *
 * \param[in] msg   Pointer to the periodic message
 * \return Periodic messages sequence number
 */
IPX_API uint64_t
ipx_msg_periodic_get_seq_num(ipx_msg_periodic_t *msg);

/**
 * \brief Get created time of periodic message
 *
 * \param[in] msg   Pointer to the periodic message
 * \return Periodic messages created time
 */
IPX_API struct timespec
ipx_msg_periodic_get_created(ipx_msg_periodic_t *msg);

/**
 * \brief Get last intermediate plugin processed time of periodic message
 *
 * \param[in] msg   Pointer to the periodic message
 * \return Periodic messages last intermediate plugin processed time
 */
IPX_API struct timespec
ipx_msg_periodic_get_last_processed(ipx_msg_periodic_t *msg);

#ifdef __cplusplus
}
#endif
#endif
