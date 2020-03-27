/**
 * @file src/core/configurator/controller.hpp
 * @author Lukas Hutak <lukas.hutak@cesnet.cz>
 * @brief Abstract class for configuration controllers (header file)
 * @date 2020
 *
 * Copyright(c) 2020 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IPFIXCOL2_CONTROLLER_BASE
#define IPFIXCOL2_CONTROLLER_BASE

#include <string>
#include <stdexcept>

#include "model.hpp"

extern "C" {
#include "../verbose.h"
#include "cpipe.h"
}

/**
 * @brief Abstract class for configuration controllers
 *
 * The purpose of this class is to provide common API for configuration and notification
 * about changes in the configuration of the collector. A controller implementation, which
 * has to provide at least a configuration model getter, should be implemented as a wrapper over
 * configuration stored in a file, database, SysRepo, etc.
 *
 * @note
 *   By default, the pipeline configurator, which is using this controller, automatically
 *   registers termination signal handlers for SIGINT and SIGTERM. If necessary, the controller
 *   is supposed to register additional handlers in start_before().
 */
class ipx_controller {
public:
    /// Forward declaration of a controller error
    class error;

    ///  Configuration status code
    enum class OP_STATUS {
        SUCCESS,  ///< Configuration operation has been successfully applied
        FAILED    ///< Configuration operation has failed (no changes)
    };

    /**
     * @brief Get configuration model of the collector
     *
     * The function is called to get the current configuration model during startup
     * or reconfiguration procedure (if supported by the controller).
     * @note It is guaranteed that this function is called for the first time after start_before()
     *   function.
     * @return Configuration model
     * @throw ipx_controller::error in case of failure (e.g. unable to get the model)
     */
    virtual ipx_config_model
    model_get() = 0;

    /**
     * @brief Function called before start of the collector
     *
     * The controller might initialize its internal structures (for example, establish connection
     * to a database, configuration repository, etc.) and register additional signal handlers.
     * @throw ipx_controller::error in case of failure
     */
    virtual void
    start_before()
    {
        IPX_DEBUG(m_name, "Configuration process has started...", '\0');
    };

    /**
     * @brief Function called after start of the collector
     * @note
     *   If startup fails, termination functions i.e. terminate_before() and terminate_after()
     *   are not called!
     * @param[in] status Operation status (success/failure)
     * @param[in] msg    Human readable result (usually a description of what went wrong)
     * @note The function should not throw any exception!
     */
    virtual void
    start_after(OP_STATUS status, std::string msg)
    {
        switch (status) {
        case OP_STATUS::SUCCESS:
            IPX_INFO(m_name, "Collector started successfully!", '\0');
            break;
        case OP_STATUS::FAILED:
            IPX_ERROR(m_name, "Collector failed to start: %s", msg.c_str());
            break;
        }
    };

    /**
     * @brief Function is called when a termination request is received
     *
     * The termination process itself cannot be stopped as there might be situations that
     * cannot be solved by the controller, such as a memory allocation error in a instance
     * of pipeline plugin etc.
     *
     * @note
     *   This function might be called multiple times before termination is complete. It can
     *   happen, for example, if another termination request is received before the termination
     *   process is complete.
     * @note
     *   The function is called only if the collector has been previously successfully initialized.
     * @note The function should not throw any exception!
     * @param[in] msg Contains information about the reason/source of termination
     */
    virtual void
    terminate_on_request(const struct ipx_cpipe_req &req, std::string msg)
    {
        (void) req;
        IPX_INFO(m_name, "Received a termination request (%s)!", msg.c_str());
    };

    /**
     * @brief Function called after termination
     *
     * All plugin instances have been already stopped and their context has been destroyed.
     * The controller might destroy its internal structures here.
     * @note The function should not throw any exception!
     */
    virtual void
    terminate_after()
    {
        IPX_DEBUG(m_name, "Termination process completed!", '\0');
    };

private:
    const char *m_name = "Configurator";
};

/**
 * @brief Controller custom error
 */
class ipx_controller::error : public std::runtime_error {
public:
    /**
     * \brief Manager error constructor (string constructor)
     * \param[in] msg An error message
     */
    explicit error(const std::string &msg) : std::runtime_error(msg) {};
    /**
     * \brief Manager error constructor (char constructor)
     * \param[in] msg An error message
     */
    explicit error(const char *msg) : std::runtime_error(msg) {};
};

#endif // IPFIXCOL2_CONTROLLER_BASE
