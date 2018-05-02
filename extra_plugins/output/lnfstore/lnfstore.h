/**
 * \file lnfstore.h
 * \author Imrich Stoffa <xstoff02@stud.fit.vutbr.cz>
 * \author Lukas Hutak <xhutak01@stud.fit.vutbr.cz>
 * \author Pavel Krobot <Pavel.Krobot@cesnet.cz>
 * \brief lnfstore plugin interface (header file)
 *
 * Copyright (C) 2015, 2016 CESNET, z.s.p.o.
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
 * This software is provided ``as is``, and any express or implied
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

#ifndef LS_LNFSTORE_H
#define LS_LNFSTORE_H

#include <stdbool.h>
#include <stdint.h>
#include <libnf.h>

#include "configuration.h"
#include "storage_basic.h"
#include "translator.h"

extern const char *msg_module;

// Size of conversion buffer
#define REC_BUFF_SIZE (65535)

/**
 * \brief Plugin instance structure
 */
struct conf_lnfstore {
    struct conf_params *params; /**< Configuration from XML file             */
    time_t window_start;        /**< Start of the current window             */

    struct {
        stg_basic_t    *basic;    /**< Store all samples                     */
        //stg_profiles_t *profiles; /**< Store records based on profiles     */
    } storage; /**< Only one type of storage is initialized at the same time */

    struct {
        lnf_rec_t *rec_ptr;       /**< LNF record (converted IPFIX record)   */
        translator_t *translator; /**< IPFIX to LNF translator               */
    } record; /**< Record conversion */
};


#endif //LS_LNFSTORE_H
