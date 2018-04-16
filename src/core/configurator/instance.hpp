/**
 * \file src/core/configurator/instance.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Pipeline instance wrappers (header file)
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

#ifndef IPFIXCOL_INSTANCE_H
#define IPFIXCOL_INSTANCE_H

#include <tuple>
#include <string>
#include <stdexcept>

#include "plugin_finder.hpp"

extern "C" {
#include <ipfixcol2.h>
#include "../fpipe.h"
#include "../ring.h"
#include "../odid_range.h"
#include "../plugin_output_mgr.h"
}

// Forward declarations
class ipx_instance;
class ipx_instance_input;
class ipx_instance_intermediate;
class ipx_instance_outmgr;
class ipx_instance_output;

/** \brief Base class of a plugin instance                                                      */
class ipx_instance {
protected:
    /** State of an instance                                                                    */
    enum class state {
        /** The instance of the plugin exists, but the constructor hasn't been called yet       */
        NEW,
        /** The instance constructor has been called, but thread hasn't been created yet        */
        INITIALIZED,
        /** The thread of the instance is running                                               */
        RUNNING
    };

    /** Status of the instance                                                                   */
    state _state;
    /** Name of the instance                                                                     */
    std::string _name;
    /** Context of the instance                                                                  */
    ipx_ctx_t *_ctx;

    /**
     * \brief Base constructor of an instance
     * \param[in] name Name of the instance
     */
    ipx_instance(const std::string &name);
    virtual ~ipx_instance() = default;

public:
    /**
     * \brief Initialize the instance
     * \param[in] params XML parameters of the instance
     * \param[in] iemgr  Reference to the manager of Information Elements
     * \param[in] level  Verbosity level
     */
    virtual void init(const std::string &params, const fds_iemgr_t *iemgr, ipx_verb_level level) = 0;

    /** \brief Start a thread of the instance                                                    */
    virtual void start() = 0;
};


/** \brief Instance of an input plugin                                                           */
class ipx_instance_input : public ipx_instance {
protected:
    /** Feedback pipe connected to the input instance                                            */
    ipx_fpipe_t *_input_feedback;

    /** Ring buffer between the instance of an input plugin and instance of the parser           */
    ipx_ring_t  *_parser_buffer;
    /** Instance of the parser plugin (internal)                                                 */
    ipx_ctx_t   *_parser_ctx;

public:
    /**
     * \brief Create an instance of an input plugin
     * \param[in] name   Name of the instance
     * \param[in] cbs    Parsed plugin interface
     * \param[in] bsize  Size of the ring buffer between the input instance and the parser instance
     */
    ipx_instance_input(const std::string &name, const struct ipx_ctx_callbacks *cbs,
        uint32_t bsize);
    /**
     * \brief Destroy the instance
     * \note
     *   If the thread is running (start() has been called), the function blocks until the thread
     *   is exited.
     */
    ~ipx_instance_input();

    // Disable copy constructors
    ipx_instance_input(const ipx_instance_input &) = delete;
    ipx_instance_input & operator=(const ipx_instance_input &) = delete;

    /**
     * \brief Initialize the instance
     *
     * Initialize a context of the input instance and the IPFIX parser.
     * \param[in] params XML parameters of the instance
     * \param[in] iemgr Reference to the manager of Information Elements
     * \param[in] level Verbosity level
     * \throw runtime_error if the function fails to initialize all components or if an output
     *   plugin is not connected
     */
    void init(const std::string &params, const fds_iemgr_t *iemgr, ipx_verb_level level);

    /**
     * \brief Start a thread of the instance
     * \throw runtime_error if a thread fails to the start
     */
    void start();

    /**
     * \brief Get a feedback pipe (for writing only)
     * \return Pointer to the pipe
     */
    ipx_fpipe_t *get_feedback();

    /**
     * \brief Connect the the input instance to an instance of an intermediate plugin
     * \param[in] intermediate Intermediate plugin to receive our messages
     */
    void connect_to(ipx_instance_intermediate &intermediate);
};


/** \brief Instance of an intermediate plugin                                                    */
class ipx_instance_intermediate : public ipx_instance {
protected:
    /** Input ring buffer                                                                        */
    ipx_ring_t *_instance_buffer;
public:
    /**
     * \brief Create an instance of an intermediate plugin
     * \param[in] name   Name of the instance
     * \param[in] cbs    Parsed plugin interface
     * \param[in] bsize  Size of the input ring buffer
     */ // TODO: multiple writers?
    ipx_instance_intermediate(const std::string &name, const struct ipx_ctx_callbacks *cbs,
        uint32_t bsize);
    /**
     * \brief Destroy the instance
     * \note
     *   If the thread is running (start() has been called), the function blocks until the thread
     *   is exited.
     */
    virtual ~ipx_instance_intermediate();

    // Disable copy constructors
    ipx_instance_intermediate(const ipx_instance_intermediate &) = delete;
    ipx_instance_intermediate &operator=(const ipx_instance_intermediate &) = delete;

    /**
     * \brief Initialize the instance
     *
     * Initialize a context of the plugin and an input ring buffer.
     * \note
     *   The instance MUST be connected connect_to() to an intermediate plugin before initialization
     * \param[in] params XML parameters of the instance
     * \param[in] iemgr  Reference to the manager of Information Elements
     * \param[in] level  Verbosity level
     * \throw runtime_error if the function fails to initialize all components or if an output
     *   plugin is not connected
     */
    void init(const std::string &params, const fds_iemgr_t *iemgr, ipx_verb_level level);

    /**
     * \brief Start a thread of the instance
     * \throw runtime_error if a thread fails to the start
     */
    void start();

    /**
     * \brief Get the input ring buffer (for writing only)
     * \warning
     *   Do NOT use if there is already another active writer and the ring buffer is not defined
     *   with enabled multi-writer mode!
     * \return Pointer to the ring buffer.
     */
    ipx_ring_t *get_input();

    /**
     * \brief Connect the the intermediate instance to another instance of an intermediate plugin
     * \param[in] intermediate Intermediate plugin to receive our messages
     */
    virtual void connect_to(ipx_instance_intermediate &intermediate);
};


/** \brief Instance of the output manager                                                        */
class ipx_instance_outmgr : public ipx_instance_intermediate {
private:
    /** List of output plugins                                                                   */
    ipx_output_mgr_list_t *_list;
    /**
     * \brief Output plugin cannot be connected to any other instance
     * \param[in] intermediate Intermediate plugin
     */
    void connect_to(ipx_instance_intermediate &intermediate);
public:
    /**
     * \brief Create an instance of the internal output manager plugin
     * \param[in] bsize  Size of the input ring buffer
     */
    explicit ipx_instance_outmgr(uint32_t bsize);
    /**
     * \brief Destroy the instance
     *   If the thread is running (start() has been called), the function blocks until the thread
     *   is exited.
     */
    ~ipx_instance_outmgr();

    // Disable copy constructors
    ipx_instance_outmgr(const ipx_instance_outmgr &) = delete;
    ipx_instance_outmgr & operator=(const ipx_instance_outmgr &) = delete;

    /**
     * \brief Initialize the instance
     *
     * Initialize a context of the plugin and an input ring buffer.
     * \note
     *   The instance MUST be connected connect_to() to at least one output plugin!
     * \param[in] params XML parameters of the instance
     * \param[in] iemgr  Reference to the manager of Information Elements
     * \param[in] level  Verbosity level
     * \throw runtime_error if the function fails to initialize all components or if no output
     *   plugins are connected.
     */
    void init(const std::string &params, const fds_iemgr_t *iemgr, ipx_verb_level level);

    /**
     * \brief Connect the the output manager to an instance of an output plugin
     * \param[in] output Output plugin to receive our messages
     * \throw runtime_error if creating of the connection fails
     */
    void connect_to(ipx_instance_output &output);
};


/** \brief Instance of the output plugin                                                         */
class ipx_instance_output : public ipx_instance {
protected:
    /** Input ring buffer                                                                        */
    ipx_ring_t *_instance_buffer;

    /** ODID filter type                                                                         */
    enum ipx_odid_filter_type _type;
    /** ODID filter (nullptr, if type == IPX_ODID_FILTER_NONE                                    */
    ipx_orange_t *_filter;
public:
    /**
     * \brief Create an instance of an output plugin
     * \param[in] name   Name of the instance
     * \param[in] cbs    Parsed plugin interface
     * \param[in] bsize  Size of the input ring buffer
     */
    ipx_instance_output(const std::string &name, const struct ipx_ctx_callbacks *cbs,
        uint32_t bsize);
    /**
     * \brief Destroy the instance
     * \note
     *   If the thread is running (start() has been called), the function blocks until the thread
     *   is exited.
     */
    ~ipx_instance_output();

    // Disable copy constructors
    ipx_instance_output(const ipx_instance_output &) = delete;
    ipx_instance_output & operator=(const ipx_instance_output &) = delete;

    /**
     * \brief Set ODID filter expression (disabled by default)
     * \param[in] type       Filter type
     * \param[in] expr Filter expression
     */
    void set_filter(ipx_odid_filter_type type, const std::string &expr);

    /**
     * \brief Initialize the instance
     *
     * Initialize a context of the plugin and an input ring buffer.
     * \note The ODID filter MUST be set before calling this initialization!
     * \param[in] params XML parameters of the instance
     * \param[in] iemgr Reference to the manager of Information Elements
     * \param[in] level Verbosity level
     * \throw runtime_error if the function fails to initialize all components or if an output
     *   plugin is not connected
     */
    void init(const std::string &params, const fds_iemgr_t *iemgr, ipx_verb_level level);

    /**
     * \brief Start a thread of the instance
     * \throw runtime_error if a thread fails to the start
     */
    void start();

    /**
     * \brief Get the input ring buffer (for writing only)
     * \warning
     *   Do NOT use if there is already another active writer.
     * \return Pointer to the ring buffer and the ODID filter.
     */
    std::tuple<ipx_ring_t *, enum ipx_odid_filter_type, const ipx_orange_t *>
    get_input();
};

#endif //IPFIXCOL_INSTANCE_H
