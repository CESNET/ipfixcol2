/**
 * \file ipfixsend/reader.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Functions for reading IPFIX file
 *
 * Copyright (C) 2016-2018 CESNET, z.s.p.o.
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

#include <stdbool.h>
#include <stdint.h>
#include <libfds.h>

#ifndef READER_H
#define	READER_H

/**
 * \brief Return status of the packet reader
 */
enum READER_STATUS {
    READER_EOF,   /**< End of File                                         */
    READER_OK,    /**< Reader operation successfully finished              */
    READER_ERROR, /**< Reader operation failed                             */
    READER_SIZE   /**< Size of a buffer is too small                       */
};

/** Data type of the packet reader                                         */
typedef struct reader_internal reader_t;

/**
 * \brief Create a new packet reader
 * \param[in] file     Path to the IPFIX file
 * \param[in] preload  Preload all IPFIX record to an internal buffer
 * \return On success returns a new pointer to instance of the reader. Otherwise
 *   returns NULL.
 */
reader_t *
reader_create(const char *file, bool preload);

/**
 * \brief Destroy a packet reader
 * \param[in] reader   Pointer to the packet reader
 */
void
reader_destroy(reader_t *reader);

/**
 * \brief Rewind file (go to the beginning of a file)
 *
 * \note
 * If header auto-update is enabled (see reader_header_autoupdate()) new offsets are calculated.
 * \param[in] reader Pointer to the packet reader
 */
void
reader_rewind(reader_t *reader);

/**
 * \brief Push the current position in a file
 *
 * Only one position in the file can be remembered. Previously unpoped position
 * in the file will be lost.
 * \param[in] reader Pointer to the packet reader
 * \return On success returns #READER_OK. Otherwise returns #READER_ERROR.
 */
enum READER_STATUS
reader_position_push(reader_t *reader);

/**
 * \brief Pop the previously pushed position in the file
 * \param[in] reader Pointer to the packet reader
 * \return On success returns #READER_OK. Otherwise returns #READER_ERROR.
 */
enum READER_STATUS
reader_position_pop(reader_t *reader);

/**
 * \brief Get the pointer to a next packet
 *
 * The function returns pointer to the next packet, which is stored in
 * an internal buffer. The position in the file is changed. A user is allowed to
 * change only a Sequence number and Observation Domain ID.
 * \warning After calling reader_get_next_packet() or reader_get_next_header()
 *   internal buffer is always overwritten with a new packet/header.
 * \param[in]  reader  Pointer to the packet reader
 * \param[out] output  Pointer to the internal buffer
 * \param[out] size    Size of the record in the buffer (can be NULL)
 * \return On success returns #READER_OK and fills the pointer to the packet
 *   (\p output) and its \p size. Otherwise returns #READER_EOF (end of file)
 *   or #READER_ERROR (malformed input file).
 */
enum READER_STATUS
reader_get_next_packet(reader_t *reader, struct fds_ipfix_msg_hdr **output,
    uint16_t *size);

/**
 * \brief Get the pointer to header of a next packet
 *
 * The function returns pointer to the next header, which is stored in
 * an internal buffer. The position in the file is changed. A user is allowed to
 * change only a Sequence number and Observation Domain ID.
 * \warning After calling reader_get_next_packet() or reader_get_next_header()
 *   internal buffer is always overwritten with a new packet/header.
 * \warning The rest of the message (behind the headers) is not available.
 * \param[in] reader  Pointer to the packet reader
 * \param[in] header  Pointer to the header
 * \return On success returns #READER_OK and fills the pointer to the \p header.
 *   Otherwise returns #READER_EOF (end of file) or #READER_ERROR (malformed
 *   input file).
 */
enum READER_STATUS
reader_get_next_header(reader_t *reader, struct fds_ipfix_msg_hdr **header);

/**
 * \brief Rewrite ODID of all IPFIX Messages
 * \param[in] reader Pointer to the packet reader
 * \param[in] odid   New ODID
 */
void
reader_odid_rewrite(reader_t *reader, uint32_t odid);

/**
 * \brief Enable/disable automatic update of IPFIX Messages
 *
 * Automatically update IPFIX Message header fields, Sequence number and Export Time, if the
 * file is replayed multiple times. If enabled, the fields are updated to follow up on the last
 * message. In other words, every time the end of file is reached, new offsets are calculated
 * and applied.
 *
 * However, the sequence number cannot be properly updated because this tool doesn't interpret
 * messages and correct number of records in the last message cannot be determined to properly
 * update sequence number. Therefore, sequence number is increase by a static value.
 *
 * If this options is enabled, every time the reader is rewinded, new offsets are calculated.
 * \note Disabling this options resets offsets from the original values.
 * \param[in] reader Pointer to the packet reader
 * \param[in] enable Enable/disable
 */
void
reader_header_autoupdate(reader_t *reader, bool enable);

#endif	/* READER_H */

