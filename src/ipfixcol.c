/**
 * \file src/ipfixcol.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Main body of IPFIXcol
 */

/* Copyright (C) 2016 CESNET, z.s.p.o.
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

#include <stdlib.h>
#include <stdio.h>

#include <ipfixcol2.h>
#include "build_config.h"

/**
 * \brief Print information about version of the collector to standard output
 */
void print_version()
{
	printf("Version:      %s\n",      IPX_BUILD_VERSION_FULL_STR);
	printf("GIT hash:     %s\n",      IPX_BUILD_GIT_HASH);
	printf("Build type:   %s\n",      IPX_BUILD_TYPE);
	printf("Architecture: %s (%s)\n", IPX_BUILD_ARCH, IPX_BUILD_BYTE_ORDER);
	printf("Compiler:     %s\n",      IPX_BUILD_COMPILER);
	printf("Compile time: %s, %s\n",  __DATE__, __TIME__);
}

/**
 * \brief Main function
 * \param[in] argc Number of arguments
 * \param[in] argv Vector of the arguments
 * \return On success returns EXIT_SUCCES. Otherwise returns EXIT_FAILURE.
 */
int main(int argc, char *argv[])
{
	print_version();
	return EXIT_SUCCESS;
}

