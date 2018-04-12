#ifndef IPFIXCOL_CONFIGURATOR_H
#define IPFIXCOL_CONFIGURATOR_H

#include <stdint.h>
#include <vector>
#include "plugin_finder.hpp"

extern "C" {
#include <ipfixcol2.h>
#include "../context.h"
}

class Configurator {
private:

public:
    /** Constructor */
    Configurator();
    /** Destructor  */
    ~Configurator();

    // Disable copy and move constructors
    Configurator(const Configurator &) = delete;
    Configurator& operator=(const Configurator &) = delete;
    Configurator(Configurator &&) = delete;
    Configurator& operator=(Configurator &&) = delete;

    /** Modules finder */
    plugin_finder finder;
};

/**
 * \brief Default instance verbosity
 * \note If the value is used, the instance uses global collector verbosity
 */
#define IPX_PLUGIN_VERB_DEFAULT 255U;

/** Output ODID filter type                                                 */
enum ipx_cfg_odid {
    /** Filter not defined (process all ODIDs)                              */
    IPX_CFG_ODID_FILTER_NONE,
    /** Process only ODIDs that match a filter                              */
    IPX_CFG_ODID_FILTER_ONLY,
    /** Process only ODIDs that do NOT match a filter                       */
    IPX_CFG_ODID_FILTER_EXCEPT,
};

/** Common plugin configuration parameters                                  */
struct ipx_plugin_common {
    /** Identification name of the plugin                                   */
    char *plugin;
    /** Identification name of the instance                                 */
    char *name;
    /** XML parameters (root node "\<param\>")                              */
    char *params;
    /**
     * Instance verbosity
     * \note If no verbosity is specified, use #IPX_PLUGIN_VERB_DEFAULT
     */
    uint8_t verb_mode;
};

/** Input plugin configuration                                              */
struct ipx_cfg_input {
    /** Common configuration                                                */
    struct ipx_plugin_common common;
};

/** Intermediate plugin configuration                                       */
struct ipx_cfg_inter {
    /** Common configuration                                                */
    struct ipx_plugin_common common;
};

/** Output plugin configuration                                             */
struct ipx_cfg_output {
    /** Common configuration                                                */
    struct ipx_plugin_common common;

    struct  {
        /** Type of ODID filter                                             */
        enum ipx_cfg_odid type;
        /**
         * Filter expression.
         * \note Can be NULL only if type == #IPX_CFG_ODID_FILTER_NONE
         */
        char *expression;
    } odid_filter; /**< ODID filter                                         */
};


/**
 * \brief Prepare for a new configuration
 *
 * This function MUST be called as the first function before any configuration attempts.
 * Among others, preparation steps includes updating of Information Elements manager.
 *
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED on failure and an error message is set (see ipx_config_last_err())
 */
IPX_API int
ipx_config_init();

/**
 * \brief Add an instance of an input plugin
 *
 * \param[in] cfg Input instance configuration
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED on failure and an error message is set (see ipx_config_last_err())
 */
IPX_API int
ipx_config_input_add(const struct ipx_cfg_input *cfg);

/**
 * \brief Add an instance of an intermediate plugin
 *
 * \param[in] cfg Intermediate instance configuration
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED on failure and an error message is set (see ipx_config_last_err())
 */
IPX_API int
ipx_config_inter_add(const struct ipx_cfg_inter *cfg);

/**
 * \brief Add an instance of an output plugin
 * \param[in] cfg Output instance configuration
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED on failure and an error message is set (see ipx_config_last_err())
 */
IPX_API int
ipx_config_output_add(const struct ipx_cfg_output *cfg);

/**
 * \brief Start new configuration
 *
 * For each defined instance try to find required plugin and check compatibility with the current
 * collector version. Then try to initialize instances by "init" callbacks (starts from the output
 * plugins, followed by intermediate plugin and finally, input plugins).
 * If any intermediate plugin fails, "delete" callbacks will be called.
 *
 *
 * \return #IPX_OK on success i.e. all instances have been successfully initialized
 * \return #IPX_ERR_DENIED on failure and an error message is set (see ipx_config_last_err())
 */
IPX_API int
ipx_config_start();

/**
 * \brief Stop the current configuration
 *
 * Send request to stop all instances of plugins and prepare for termination. After returning from
 * this function no instances are running.
 */
IPX_API void
ipx_config_stop();

/**
 * \brief Get the last error message
 * \return
 */
IPX_API const char *
ipx_config_last_err();


#endif //IPFIXCOL_CONFIGURATOR_H
