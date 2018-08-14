/**
 * \file src/plugins/output/json/src/Storage.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief JSON converter and output manager (header file)
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

#ifndef JSON_STORAGE_H
#define JSON_STORAGE_H

#include <string>
#include <vector>
#include <ipfixcol2.h>
#include "Config.hpp"

/** Base class                                                                                   */
class Output {
protected:
    /** Identification name of the output                                                        */
    std::string _name;
    /** Instance context (only for messages)                                                     */
    ipx_ctx_t *_ctx;
public:
    /**
     * \brief Base class constructor
     * \param[in] id  Identification of the output
     * \param[in] ctx Instance context
     */
    Output(const std::string &id, ipx_ctx_t *ctx) : _name(id), _ctx(ctx) {};
    /** \brief Base class destructor                                                             */
    virtual
    ~Output() {};

    /**
     * \brief Process a converted JSON
     * \param[in] str JSON Record
     * \param[in] len Length of the record (excluding the terminating null byte '\0')
     * \return #IPX_OK on success
     * \return #IPX_ERR_DENIED in case of a fatal error (the output cannot continue)
     */
    virtual int
    process(const char *str, size_t len) = 0;
};

/** JSON converter and output manager                                                            */
class Storage {
private:
    /** Converter function pointer                                                               */
    typedef void (Storage::*converter_fn)(const struct fds_drec_field &);
    /** Registered outputs                                                                       */
    std::vector<Output *> outputs;
    /** Formatting options                                                                       */
    struct cfg_format format;

    struct {
        /** Data begin                                                                           */
        char *buffer_begin;
        /** The past-the-end element (a character that would follow the last character)          */
        char *buffer_end;

        /** Position of the next write operation                                                 */
        char *write_begin;
    } record; /**< Converted JSON record                                                         */

    /** Size of an array for formatting names of unknown fields                                  */
    static constexpr size_t raw_size = 32;
    /** Array for formatting name of unknown fields                                             */
    char raw_name[raw_size];

    // Find a conversion function for an IPFIX field
    converter_fn get_converter(const struct fds_drec_field &field);
    // Convert an IPFIX record to a JSON string
    void convert(struct fds_drec &rec);

    // Remaining buffer size
    size_t buffer_remain() const {return record.buffer_end - record.write_begin;};
    // Total size of allocated buffer
    size_t buffer_alloc() const {return record.buffer_end - record.buffer_begin;};
    // Used buffer size
    size_t buffer_used() const {return record.write_begin - record.buffer_begin;};
    // Append append a string
    void buffer_append(const char *str);
    // Reserve memory for a JSON string
    void buffer_reserve(size_t size);

    // Add a field name
    void add_field_name(const struct fds_drec_field &field);

    // Conversion functions
    void to_octet(const struct fds_drec_field &field);
    void to_int(const struct fds_drec_field &field);
    void to_uint(const struct fds_drec_field &field);
    void to_float(const struct fds_drec_field &field);
    void to_bool(const struct fds_drec_field &field);
    void to_mac(const struct fds_drec_field &field);
    void to_ip(const struct fds_drec_field &field);
    void to_string(const struct fds_drec_field &field);
    void to_datetime(const struct fds_drec_field &field);
    void to_flags(const struct fds_drec_field &field);
    void to_proto(const struct fds_drec_field &field);

public:
    /**
     * \brief Constructor
     * \param[in] fmt Conversion specifier
     */
    explicit Storage(const struct cfg_format &fmt);
    /** Destructor */
    ~Storage();

    /**
     * \brief Add a new output instance
     *
     * Every time a new record is converted, the output instance will receive a reference
     * to the record and store it.
     * \note The storage will destroy the output instance when during destruction of this storage
     * \param[in] output Instance to add
     */
    void
    output_add(Output *output);

    /**
     * \brief Process IPFIX Message records
     *
     * For each record perform conversion to JSON and pass it to all output instances.
     * \param[in] msg IPFIX Message to convert
     * \return #IPX_OK on success
     * \return #IPX_ERR_DENIED if a fatal error has occurred and the storage cannot continue to
     *   work properly!
     */
    int
    records_store(ipx_msg_ipfix_t *msg);
};


#endif // JSON_STORAGE_H
