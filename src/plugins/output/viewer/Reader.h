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
#define WRITER_SIZE_SPACE 6
/**
 * \brief Spaces in output for Field name
 */
#define WRITER_FIELD_NAME_SPACE 35
/**
 * \brief Spaces in output for Organization name
 */
#define WRITER_ORG_NAME_SPACE 12

/**
 * \brief Print all data of an IPFIX Message
 *
 * Function reads and prints the header of the packet and then iterates through the Sets and their
 * records. Information printed from the header of the packet are:
 * version, length, export time, sequence number and Observation Domain ID
 * \param[in] msg   IPFIX message which will be printed
 * \param[in] iemgr Information Element manager
 */
void
read_packet(ipx_msg_ipfix_t *msg, const fds_iemgr_t *iemgr);

/**
 * \brief Print all values inside the single Data Record of an IPFIX message
 *
 * Reads and prints the header of the Data Record and then iterates through its fields.
 * \param[in] rec    Record which will be printed
 * \param[in] indent Additional output indentation
 * \param[in] iemgr  Information Element manager
 */
void
read_record(struct fds_drec *rec, unsigned int indent, const fds_iemgr_t *iemgr);

/**
 * \brief Print the value of the Data Record field
 *
 * Reads and prints all information about the data in the field. If the detailed definition is
 * known, value is printed in human readable format. Otherwise data are printed in the raw
 * format (hexadecimal). In both cases Enterprise number and ID will be printed.
 * \param[in] field  Field which will be printed
 * \param[in] indent Additional output indentation
 * \param[in] iemgr  Information Element manager
 * \param[in] snap   Template snapshot of the Data Record
 */
void
read_field(struct fds_drec_field *field, unsigned int indent, const fds_iemgr_t *iemgr,
    const fds_tsnapshot_t *snap);

/**
 * \brief Print the content of basicList data type
 *
 * Iterates through the list and prints all values.
 * \param[in] field  Field which will be printed
 * \param[in] indent Additional output indentation
 * \param[in] iemgr  Information Element manager
 * \param[in] snap   Template snapshot of the Data Record
 */
void
read_list_basic(struct fds_drec_field *field, unsigned int indent, const fds_iemgr_t *iemgr,
    const fds_tsnapshot_t *snap);

/**
 * \brief Print the content of subTemplateList data type
 *
 * Iterates through the list and prints all Data Records.
 * \param[in] field  Field which will be printed
 * \param[in] indent Additional output indentation
 * \param[in] iemgr  Information Element manager
 * \param[in] snap   Template snapshot of the Data Record
 */
void
read_list_stl(struct fds_drec_field *field, unsigned int indent, const fds_iemgr_t *iemgr,
    const fds_tsnapshot_t *snap);

/**
 * \brief Print the content of subTemplateMultiList data type
 *
 * Iterates through the list and prints all top-level lists and their content i.e. Data Records.
 * \param[in] field  Field which will be printed
 * \param[in] indent Additional output indentation
 * \param[in] iemgr  Information Element manager
 * \param[in] snap   Template snapshot of the Data Record
 */
void
read_list_stml(struct fds_drec_field *field, unsigned int indent, const fds_iemgr_t *iemgr,
    const fds_tsnapshot_t *snap);

/**
 * \brief Print the (Options) Template Record
 *
 * Reads and prints content of the (Options) Template Record. Uses Information Element manager
 * for determine description of present Information Elements.
 * \param[in] tset_iter Template iterator
 * \param[in] set_id    ID of the Set to which the record belongs
 * \param[in] iemgr     Information Element manager
 */
void
read_template_set(struct fds_tset_iter *tset_iter, uint16_t set_id, const fds_iemgr_t *iemgr);

/**
 * \brief Print the IPFIX Set and its content
 *
 * Reads and prints single IPFIX Set and determines what kind of IPFIX Set it is and which
 * read_xxx function to use for reading its content.
 *
 * \param[in]     sets_iter Sets iterator
 * \param[in]     msg       IPFIX message
 * \param[in]     iemgr     Information Element manager
 * \param[in/out] rec_i     Number of processed Data Records
 */
void
read_set(struct ipx_ipfix_set *set, ipx_msg_ipfix_t *msg, const fds_iemgr_t *iemgr, uint32_t *rec_i);

#endif //IPFIXCOL_READER_H
