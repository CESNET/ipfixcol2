/**
 * \file src/plugins/output/json/src/Printer.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Printer to standard output (header file)
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

#ifndef JSON_PRINTER_H
#define JSON_PRINTER_H

#include "Config.hpp"
#include "Storage.hpp"

/** Printer to standard output */
class Printer : public Output {
public:
    /**
     * \brief Constructor
     * \param[in] cfg Output configuration
     * \param[in] ctx Instance context
     */
    explicit Printer(const struct cfg_print &cfg, ipx_ctx_t *ctx);

    /**
     * \brief Print a record on standard output
     * \param[in] str JSON string to print
     * \param[in] len Length of the string
     * \return #IPX_OK on success
     * \return #IPX_ERR_DENIED in case of fatal failure
     */
    int process(const char *str, size_t len);
};

#endif // JSON_PRINTER_H
