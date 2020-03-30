/**
 * \file src/core/configurator/instance.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Pipeline instance wrappers (header file)
 * \date 2018-2020
 */

/* Copyright (C) 2018-2020 CESNET, z.s.p.o.
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
#include <memory>
#include "plugin_mgr.hpp"
#include "extensions.hpp"

extern "C" {
#include <ipfixcol2.h>
#include "../ring.h"
}

/** Unique pointer type of an instance context */
using unique_ctx = std::unique_ptr<ipx_ctx_t, decltype(&ipx_ctx_destroy)>;
/** Unique pointer type of a ring buffer       */
using unique_ring = std::unique_ptr<ipx_ring_t, decltype(&ipx_ring_destroy)>;

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
    /** Reference to the plugin description and callbacks (can be nullptr)                       */
    ipx_plugin_mgr::plugin_ref *_plugin_ref;

    /**
     * \brief Base constructor of an instance
     *
     * \note
     *   The \p ref is plugin reference wrapper of the plugin. The wrapper helps to monitor number
     *   of plugins that use the plugin. The reference will be destroyed during this destruction
     *   of the object of this class.
     * \param[in] name Name of the instance
     * \param[in] ref  Reference to the plugin (will be automatically delete on destroy)
     */
    ipx_instance(const std::string &name, ipx_plugin_mgr::plugin_ref *ref)
        : _state(state::NEW), _name(name), _ctx(nullptr), _plugin_ref(ref) {};
    /**
     * \brief Base destructor of an instance
     *
     * Destroys the plugin reference
     */
    virtual ~ipx_instance() {delete _plugin_ref;};
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

    /**
     * \brief Get name of the instance
     * \return Name
     */
    const std::string &
    get_name() {
        return _name;
    }

    /**
     * \brief Registered extensions and dependencies
     * \param[in] ext_mgr Extension manager
     */
    virtual void
    extensions_register(ipx_cfg_extensions *ext_mgr, size_t pos) {
        ext_mgr->register_instance(_ctx, pos);
    }

    /**
     * \brief Resolve definition of the extension/dependency definitions of the instance
     *
     * Description of each extension/dependency is updated to contain size, offset, etc.
     * \param[in] ext_mgr Extension manager
     */
    virtual void
    extensions_resolve(ipx_cfg_extensions *ext_mgr) {
        ext_mgr->update_instance(_ctx);
    }

    /**
     * \brief Enable/disable processing of data messages (IPFIX and Transport Session)
     * \note By default, data processing is enabled.
     * \see ipx_ctx_processing() for more details
     * \param[in] en Enable/disable processing
     */
    virtual void
    set_processing(bool en) {
        ipx_ctx_processing_set(_ctx, en);
    }
};

#endif //IPFIXCOL_INSTANCE_H
