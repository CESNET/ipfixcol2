/*
 * \file ipfixcol.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief The main devel header for IPFIX Collector
 *
 * Copyright (C) 2016 CESNET, z.s.p.o.
 *
 * LICENSE TERMS
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

#ifndef IPFIXCOL2_H_
#define IPFIXCOL2_H_

/**
 * \mainpage IPFIX Collector Developer's Documentation
 *
 * This documents provides documentation of IPFIX Collector (IPFIXcol). We
 * provide public API of the collector's plugins.
 */

/**
 * \defgroup publicAPIs Public IPFIXcol's APIs
 * \brief IPFIXcol API for plugins.
 */

#include <ipfixcol2/api.h>

#include <ipfixcol2/message.h>
#include <ipfixcol2/message_garbage.h>
#include <ipfixcol2/message_session.h>
#include <ipfixcol2/message_ipfix.h>

#include <ipfixcol2/plugins.h>
#include <ipfixcol2/session.h>
#include <ipfixcol2/utils.h>
#include <ipfixcol2/verbose.h>


#endif /* IPFIXCOL2_H_ */
