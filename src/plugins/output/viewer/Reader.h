/**
 * \file src/plugins/output/viewer/Reader.h
 * \author Jan Kala <xkalaj01@stud.fit.vutbr.cz>
 * \brief Viewer - output module for printing information about incoming packets on stdout (header file)
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
#ifndef IPFIXCOL_READER_H
#define IPFIXCOL_READER_H

#include <ipfixcol2.h>

/**
 * \brief spaces in output for Enterprise number field
 */
#define WRITER_EN_SPACE 8
/**
 * \brief Spaces in output for ID field
 */
#define WRITER_ID_SPACE 6
/**
 * \brief Spaces in output for size field
 */
#define WRITER_SIZE_SPACE 7
/**
 * \brief Spaces in output for Field name
 */
#define WRITER_FIELD_NAME_SPACE 35
/**
 * \brief Spaces in output for Organization name
 */
#define WRITER_ORG_NAME_SPACE 12

/**
 * \brief Reads the data inside of ipfix message
 *
 * Function reads and prints header of the packet and then iterates through the records.
 * Information printed from header of packet are:
 * version, length, export time, sequence number and Observation Domain ID
 * \param[in] msg ipfix message which will be printed
 */
void
read_packet(ipx_msg_ipfix_t *msg, const fds_iemgr_t *iemgr);

/**
 * \brief Reads data inside the single record of IPFIX message
 *
 * Reads and prints the header of the record and then iterates through the fields.
 * Information printed from header of record are:
 * Template ID and number of the fields inside the record
 * \param[in] rec record which will be printed
 */
void
read_record(struct fds_drec *rec, unsigned int indent, const fds_iemgr_t *iemgr);

/**
 * \brief Reads the data inside the field of IPFIX message
 *
 * Reads and prints all the information about the data in the field
 * if the detailed definition is known, data are printed in readable format
 * as well as the information about the data (organisation name and name of the data).
 * Otherwise data are printed in the raw format (hexadecimal)
 * In both cases Enterprise number and ID will be printed.
 * \param[in] field Field which will be printed
 */
void
read_field(struct fds_drec_field *field, unsigned int indent, const fds_iemgr_t *iemgr, const fds_tsnapshot_t *snap, bool in_basicList);

/**
 * \brief Reads set which contains template
 *
 * Reads template type of sets which has set id == 2 or set id == 3. Uses Information Element manager from
 * parameters for parsing the data inside of the template.
 * \param tset_iter Template iterator
 * \param set_id ID of the whole set
 * \param iemgr IE Manager
 */
void
read_template_set(struct fds_tset_iter *tset_iter, uint16_t set_id, const fds_iemgr_t *iemgr);

/**
 * \brief Reads single set
 *
 * Reads single set and determines what kind of set it is and which read_xxx function to use for reading the fields.
 *
 * \param sets_iter Sets iterator
 * \param msg  IPFix message
 * \param iemgr  IE Manager for parsing the fields
 * \param rec_i Number of record
 */
void
read_set(struct ipx_ipfix_set *set, ipx_msg_ipfix_t *msg, const fds_iemgr_t *iemgr, uint32_t *rec_i);

#endif //IPFIXCOL_READER_H
