/**
 * \file ipfixsend/reader.c
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "reader.h"

/** Maximum IPFIX packet size (2^16) */
#define MAX_PACKET_SIZE 65536
/** Increase the auto-updated sequence number after finishing the file       */
#define SEQ_NUM_INC     256

/** Auto update configuration */
struct reader_autoupdate {
    bool enable;      /**< Enable auto update of header                      */
    bool file_start;  /**< Current record is the first in the file           */
    bool read_start;  /**< Current record is the first ever sent             */

    uint32_t seq_off; /**< Current Sequence Number offset                    */
    uint32_t exp_off; /**< Current Export Time offset                        */

    uint32_t seq_max; /**< Max. sequence number                              */
    uint32_t exp_max; /**< Max. Export timestamp                             */
};

/** Internal representation of the packet reader                             */
struct reader_internal {
    FILE *file;          /**< Input file                                     */
    size_t next_id;      /**< Index of next packet                           */
    bool is_preloaded;   /**< Is the whole file preloaded                    */

    struct fds_ipfix_msg_hdr **packets_preload;  /**< Preloaded packets      */
    uint8_t packet_single[MAX_PACKET_SIZE];      /**< Internal buffer        */

    struct {
        struct {
            uint32_t value; /**< New value                                   */
            bool rewrite;   /**< Enable                                      */
        } odid;             /**< ODID                                        */

        /** Auto update of IPFIX Message headers                             */
        struct reader_autoupdate update;
    } rewrite;             /**< Rewrite header parameters                    */

    struct {
        bool   valid;      /**< Position validity flag                       */
        fpos_t pos_offset; /**< Position (only for non-preloaded)            */
        size_t pos_idx;    /**< Position (only for preloaded)                */
    } pos;  /**< Pushed position in the file */
};

// Function prototypes
static struct fds_ipfix_msg_hdr **
reader_preload_packets(reader_t *reader);
static void
reader_free_preloaded_packets(struct fds_ipfix_msg_hdr **packets);


// Create a new packet reader
reader_t *
reader_create(const char *file, bool preload)
{
    reader_t *new_reader = calloc(1, sizeof(*new_reader));
    if (!new_reader) {
        fprintf(stderr, "Unable to allocate memory (%s:%d)!\n", __FILE__, __LINE__);
        return NULL;
    }

    new_reader->file = fopen(file, "rb");
    if (!new_reader->file) {
        fprintf(stderr, "Unable to open input file '%s': %s\n", file,
            strerror(errno));
        free(new_reader);
        return NULL;
    }

    new_reader->is_preloaded = preload;
    if (preload) {
        new_reader->packets_preload = reader_preload_packets(new_reader);
        if (new_reader->packets_preload == NULL) {
            fclose(new_reader->file);
            free(new_reader);
            return NULL;
        }

        // We don't need file anymore...
        fclose(new_reader->file);
        new_reader->file = NULL;
    }

    return new_reader;
}


// Destroy a packet reader
void
reader_destroy(reader_t *reader)
{
    if (!reader) {
        return;
    }

    if (reader->is_preloaded) {
        reader_free_preloaded_packets(reader->packets_preload);
    }

    if (reader->file) {
        fclose(reader->file);
    }

    free(reader);
}

/**
 * \brief Compare IPFIX 32bit header numbers (with wraparound support)
 *
 * \note The purpose of the function is to compare Sequence numbers and Export timestamps
 * \param[in] t1 First timestamp
 * \param[in] t2 Second timestamp
 * \return  The function  returns an integer less than, equal to, or greater than zero if the
 *   first number \p t1 is found, respectively, to be less than, to match, or be greater than
 *   the second number.
 */
static inline int
reader_u32_cmp(uint32_t t1, uint32_t t2)
{
    if (t1 == t2) {
        return 0;
    }

    if ((t1 - t2) & 0x80000000) { // test the "sign" bit
        return (-1);
    } else {
        return 1;
    }
}

/**
 * \brief Read the the IPFIX header from a file
 * \param[in]  reader Pointer to the reader
 * \param[out] header IPFIX header
 * \return On success returns #READER_OK and fills the \p header. When
 *   end-of-file occurs returns #READER_EOF. Otherwise (failed to read
 *   the header, malformed header) returns #READER_ERROR and the content of
 *   the \p header is undefined.
 */
static enum READER_STATUS
reader_load_packet_header(reader_t *reader, struct fds_ipfix_msg_hdr *header)
{
    size_t read_size = fread(header, 1, FDS_IPFIX_MSG_HDR_LEN, reader->file);
    if (read_size != FDS_IPFIX_MSG_HDR_LEN) {
        if (read_size == 0 && feof(reader->file)) {
            return READER_EOF;
        } else {
            fprintf(stderr, "Unable to read a packet header (probably "
                "malformed packet).\n");
            return READER_ERROR;
        }
    }

    if (ntohs(header->version) != FDS_IPFIX_VERSION) {
        fprintf(stderr, "Invalid version of a packet header.\n");
        return READER_ERROR;
    }

    uint16_t new_size = ntohs(header->length);
    if (new_size < FDS_IPFIX_MSG_HDR_LEN) {
        fprintf(stderr, "Invalid size a packet in the packet header.\n");
        return READER_ERROR;
    }

    return READER_OK;
}

/**
 * \brief Load the next packet from a file to a user defined buffer
 *
 * Load message from the the file and store it into the \p out_buffer.
 * If the size of the buffer (\p out_size) is too small, the function will fail
 * the position in the file is unchanged and a content of the buffer is
 * undefined.
 * \param[in]     reader      Pointer to the packet reader
 * \param[in,out] out_buffer  Pointer to the output buffer
 * \param[in,out] out_size    Size of the output buffer
 * \return On success returns #READER_OK, fills the \p output_buffer and
 *   \p out_size. When the size of the buffer is too small, returns
 *   #READER_SIZE and the buffer is unchanged. When end-of-file occurs, returns
 *   #READER_EOF. Otherwise (i.e. malformed packet) returns #READER_ERROR.
 */
static enum READER_STATUS
reader_load_packet_buffer(reader_t *reader, uint8_t *out_buffer,
    size_t *out_size)
{
    // Load the packet header
    struct fds_ipfix_msg_hdr header;
    enum READER_STATUS status = reader_load_packet_header(reader, &header);
    if (status != READER_OK) {
        return status;
    }

    uint16_t new_size = ntohs(header.length);
    if (*out_size < new_size) {
        if (fseek(reader->file, -FDS_IPFIX_MSG_HDR_LEN, SEEK_CUR)) {
            fprintf(stderr, "fseek error: %s\n", strerror(errno));
            return READER_ERROR;
        }
        return READER_SIZE;
    }

    // Load the rest of the packet
    memcpy(out_buffer, (const uint8_t *) &header, FDS_IPFIX_MSG_HDR_LEN);
    uint8_t *body_ptr = out_buffer + FDS_IPFIX_MSG_HDR_LEN;
    uint16_t body_size = new_size - FDS_IPFIX_MSG_HDR_LEN;

    if (body_size > 0 && fread(body_ptr, body_size, 1, reader->file) != 1) {
        fprintf(stderr, "Unable to read a packet!\n");
        return READER_ERROR;
    }

    *out_size = new_size;
    return READER_OK;
}

/**
 * \brief Allocate a memory large enough and load the next packet from a file
 *   into it.
 *
 * User MUST manually free the packet later.
 * \note You should check EOF before calling this function.
 * \param[in] reader        Pointer to the packet reader
 * \param[out] packet_data  Pointer to newly allocated packet
 * \param[out] packet_size  Size of the new packet (can be NULL)
 * \return On success returns #READER_OK and fills the \p packet_data and
 *   \p packet_size. When end-of-file occurs, returns #READER_EOF. Otherwise
 *   (i.e. malformed packet, memory allocation error) returns #READER_ERROR.
 */
static enum READER_STATUS
reader_load_packet_alloc(reader_t *reader, struct fds_ipfix_msg_hdr **packet_data,
    uint16_t *packet_size)
{
    // Load the packet header
    struct fds_ipfix_msg_hdr header;
    uint8_t *result;

    enum READER_STATUS status = reader_load_packet_header(reader, &header);
    if (status != READER_OK) {
        return status;
    }

    uint16_t new_size = ntohs(header.length);
    result = (uint8_t *) malloc(new_size);
    if (!result) {
        fprintf(stderr, "Unable to allocate memory (%s:%d)!\n", __FILE__, __LINE__);
        return READER_ERROR;
    }

    memcpy(result, (const uint8_t *) &header, FDS_IPFIX_MSG_HDR_LEN);
    uint8_t *body_ptr = result + FDS_IPFIX_MSG_HDR_LEN;
    uint16_t body_size = new_size - FDS_IPFIX_MSG_HDR_LEN;

    if (body_size > 0 && fread(body_ptr, body_size, 1, reader->file) != 1) {
        fprintf(stderr, "Unable to read a packet!\n");
        free(result);
        return READER_ERROR;
    }

    if (packet_size) {
        *packet_size = new_size;
    }

    *packet_data = (struct fds_ipfix_msg_hdr *) result;
    return READER_OK;
}

/**
 * \brief Read packets from IPFIX file and store them into memory
 *
 * Result represents a NULL-terminated array of pointers to the packets.
 * \param[in] reader  Pointer to the packet reader
 * \return On success returns a pointer to the array. Otherwise returns NULL.
 */
static struct fds_ipfix_msg_hdr **
reader_preload_packets(reader_t *reader)
{
    enum READER_STATUS status;
    size_t pkt_cnt = 0;
    size_t pkt_max = 2048;

    struct fds_ipfix_msg_hdr **packets = calloc(pkt_max, sizeof(*packets));
    if (!packets) {
        fprintf(stderr, "Unable to allocate memory (%s:%d)!\n", __FILE__, __LINE__);
        return NULL;
    }

    while (1) {
        // Read packet
        status = reader_load_packet_alloc(reader, &packets[pkt_cnt], NULL);
        if (status == READER_EOF) {
            break;
        }

        if (status != READER_OK) {
            // Failed -> Delete all loaded packets
            for (size_t i = 0; i < pkt_cnt; ++i) {
                free(packets[i]);
            }

            free(packets);
            return NULL;
        }

        // Move array index to next packet - resize array if needed
        pkt_cnt++;
        if (pkt_cnt < pkt_max) {
            continue;
        }

        size_t new_max = 2 * pkt_max;
        struct fds_ipfix_msg_hdr** new_packets;

        new_packets = realloc(packets, new_max * sizeof(*packets));
        if (!new_packets) {
            fprintf(stderr, "Unable to allocate memory (%s:%d)!\n", __FILE__, __LINE__);
            // Failed -> Delete all loaded packets
            for (size_t i = 0; i < pkt_cnt; ++i) {
                free(packets[i]);
            }

            free(packets);
            return NULL;
        }

        packets = new_packets;
        pkt_max = new_max;
    }

    packets[pkt_cnt] = NULL;
    return packets;
}

/**
 * \brief Free allocated memory for packets
 * \param[in] packets Array of packets
 */
static void
reader_free_preloaded_packets(struct fds_ipfix_msg_hdr **packets)
{

    for (size_t i = 0; packets[i] != NULL; ++i) {
        free(packets[i]);
    }

    free(packets);
}


#include <inttypes.h>

static void
reader_header_update(reader_t *reader, struct fds_ipfix_msg_hdr *hdr)
{
    // Update ODID
    if (reader->rewrite.odid.rewrite) {
        hdr->odid = htonl(reader->rewrite.odid.value);
    }

    struct reader_autoupdate *update = &reader->rewrite.update;
    if (!update->enable) {
        return;
    }

    // Update header
    if (update->file_start) {
        // Calculate new offsets
        update->file_start = false;

        if (update->read_start) {
            // This is the first record from the 1. iteration (no offsets)
            update->read_start = false;
            update->seq_off = 0;
            update->exp_off = 0;
            update->seq_max = ntohl(hdr->seq_num);
            update->exp_max = ntohl(hdr->export_time);
        } else {
            // This is the first record from the 2..n iteration
            update->seq_off = update->seq_max - ntohl(hdr->seq_num) + SEQ_NUM_INC;
            update->exp_off = update->exp_max - ntohl(hdr->export_time) + 1;
            update->seq_max = ntohl(hdr->seq_num) + update->seq_off;
            update->exp_max = ntohl(hdr->export_time) + update->exp_off;
        }
    }

    uint32_t new_exp = ntohl(hdr->export_time) + update->exp_off;
    uint32_t new_seq = ntohl(hdr->seq_num) + update->seq_off;
    hdr->export_time = htonl(new_exp);
    hdr->seq_num = htonl(new_seq);

    // Update max. values
    if (reader_u32_cmp(update->exp_max, new_exp) < 0) {
        update->exp_max = new_exp;
    }

    if (reader_u32_cmp(update->seq_max, new_seq) < 0) {
        update->seq_max = new_seq;
    }
}

// Rewind file (go to the beginning of a file)
void
reader_rewind(reader_t *reader)
{
    if (reader->is_preloaded) {
        reader->next_id = 0;
    } else {
        rewind(reader->file);
    }

    reader->rewrite.update.file_start = true;
}

// Push the current position in a file
enum READER_STATUS
reader_position_push(reader_t *reader)
{
    reader->pos.valid = false;
    if (reader->is_preloaded) {
        reader->pos.pos_idx = reader->next_id;
    } else {
        if (fgetpos(reader->file, &reader->pos.pos_offset)) {
            fprintf(stderr, "fgetpos() error: %s\n", strerror(errno));
            return READER_ERROR;
        }
    }

    reader->pos.valid = true;
    return READER_OK;
}

// Pop the previously pushed position in the file
enum READER_STATUS
reader_position_pop(reader_t *reader)
{
    if (reader->pos.valid == false) {
        fprintf(stderr, "Internal Error: reader_position_pop()\n");
        return READER_ERROR;
    }

    reader->pos.valid = false;
    if (reader->is_preloaded) {
        reader->next_id = reader->pos.pos_idx;
    } else {
        if (fsetpos(reader->file, &reader->pos.pos_offset)) {
            fprintf(stderr, "fsetpos() error: %s\n", strerror(errno));
            return READER_ERROR;
        }
    }

    return READER_OK;
}

// Get the pointer to a next packet
enum READER_STATUS
reader_get_next_packet(reader_t *reader, struct fds_ipfix_msg_hdr **output, uint16_t *size)
{
    // Store data into the internal buffer
    struct fds_ipfix_msg_hdr *buffer;
    buffer = (struct fds_ipfix_msg_hdr *) reader->packet_single;

    if (reader->is_preloaded) {
        // Read from memory
        const struct fds_ipfix_msg_hdr *packet = reader->packets_preload[reader->next_id];
        if (packet == NULL) {
            return READER_EOF;
        }

        // Copy to the buffer
        memcpy(buffer, packet, ntohs(packet->length));
        ++reader->next_id;
    } else {
        // Read from the file
        size_t b_size = MAX_PACKET_SIZE;
        enum READER_STATUS ret;

        ret = reader_load_packet_buffer(reader, (uint8_t *) buffer, &b_size);
        if (ret == READER_EOF) {
            return READER_EOF;
        }

        if (ret != READER_OK) {
            // Buffer should be big enough, so only an error can occur
            return READER_ERROR;
        }
    }

    reader_header_update(reader, buffer);
    *output = buffer;
    if (size) {
        *size = ntohs(buffer->length);
    }
    return READER_OK;
}

// Get the pointer to header of a next packet
enum READER_STATUS
reader_get_next_header(reader_t *reader, struct fds_ipfix_msg_hdr **header)
{
    // Store data into the internal buffer
    struct fds_ipfix_msg_hdr *header_buffer;
    header_buffer = (struct fds_ipfix_msg_hdr *) reader->packet_single;

    if (reader->is_preloaded) {
        // Read from memory
        const struct fds_ipfix_msg_hdr *packet = reader->packets_preload[reader->next_id];
        if (packet == NULL) {
            return READER_EOF;
        }

        // Copy to the buffer
        memcpy(header_buffer, packet, FDS_IPFIX_MSG_HDR_LEN);
        ++reader->next_id;
    } else {
        // Read from the file
        enum READER_STATUS status;
        status = reader_load_packet_header(reader, header_buffer);
        if (status != READER_OK) {
            return status;
        }

        // Seek to the next header
        uint16_t packet_size = ntohs(header_buffer->length);
        uint16_t body_size = packet_size - FDS_IPFIX_MSG_HDR_LEN;

        if (fseek(reader->file, (long) body_size, SEEK_CUR)) {
            fprintf(stderr, "fseek error: %s\n", strerror(errno));
            return READER_ERROR;
        }
    }

    reader_header_update(reader, header_buffer);
    *header = header_buffer;
    return READER_OK;
}

void
reader_odid_rewrite(reader_t *reader, uint32_t odid)
{
    reader->rewrite.odid.value = odid;
    reader->rewrite.odid.rewrite = true;
}

void
reader_header_autoupdate(reader_t *reader, bool enable)
{
    struct reader_autoupdate *update = &reader->rewrite.update;
    memset(update, 0, sizeof(*update));
    update->enable = enable;
    update->file_start = true;
    update->read_start = true;
}