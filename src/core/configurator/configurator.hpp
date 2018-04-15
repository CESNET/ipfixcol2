#ifndef IPFIXCOL_CONFIGURATOR_H
#define IPFIXCOL_CONFIGURATOR_H

#include <stdint.h>
#include <vector>
#include "plugin_finder.hpp"
#include "model.hpp"

extern "C" {
#include <ipfixcol2.h>
#include "../context.h"
}

/** Main configurator of the internal pipeline                                                */
class ipx_configurator {
private:
    /** Size of ring buffers                                                                   */
    uint32_t ring_size;
    /** Directory with definitions of Information Elements                                     */
    std::string iemgr_dir;

    /** Current configuration model                                                            */
    ipx_config_model *running_model;
    /** Manager of Information Elements                                                        */
    fds_iemgr_t *iemgr;

public:
    /** Minimal size of ring buffers between instances of plugins                              */
    static constexpr uint32_t RING_MIN_SIZE = 128;
    /** Default size of ring buffers between instances of plugins                              */
    static constexpr uint32_t RING_DEF_SIZE = 8096;

    /** Constructor */
    ipx_configurator();
    /** Destructor  */
    ~ipx_configurator();

    // Disable copy constructors
    ipx_configurator(const ipx_configurator &) = delete;
    ipx_configurator& operator=(const ipx_configurator &) = delete;

    /** Modules finder */
    ipx_plugin_finder finder;

    /**
     * \brief Apply new configuration model
     *
     * // TODO: try to load ... elements first?
     * \param model Configuration model
     *
     * TODO: throw
     */
    void apply(const ipx_config_model &model);

    /**
     * \brief Define a path to the directory of Information Elements definitions
     * \param[in] path Path
     */
     void set_iemgr_dir(const std::string path);
     /**
      * \brief Define a size of ring buffers
      * \param[in] size Size
      */
     void set_buffer_size(uint32_t size);
};


#endif //IPFIXCOL_CONFIGURATOR_H
