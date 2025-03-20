/**
 * @file
 * @author Adrian Duriska <xdurisa00@stud.fit.vutbr.cz>
 * @brief Periodic message
 *
 * Copyright: (C) 2024 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IPFIXCOL_MESSAGE_PERIODIC_INTERNAL_H
#define IPFIXCOL_MESSAGE_PERIODIC_INTERNAL_H

#include <ipfixcol2.h>
#include "message_base.h"

/**
 * \brief Create a periodic message
 *
 * \param[in,out] seq   Pointer to the periodic messages sequence counter
 * \return On success returns a pointer to the message. Otherwise returns NULL.
 */
ipx_msg_periodic_t *
ipx_msg_periodic_create(uint64_t seq);

/**
 * \brief Destroy a periodic message
 *
 * \param[in] msg   Pointer to the periodic message to be destroyed
 */
void
ipx_msg_periodic_destroy(ipx_msg_periodic_t *msg);

/**
 * \brief Set last intermediate processed time
 */
void
ipx_msg_periodic_update_last_processed(ipx_msg_periodic_t *msg);

#endif
