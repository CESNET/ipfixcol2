/**
 * \file include/ipfixcol2/plugins.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief IPFIXcol plugin interface and functions (header file)
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

#ifndef IPX_PLUGINS_H
#define IPX_PLUGINS_H

#ifdef __cplusplus
extern "C" {
#endif

/** Internal plugin context                                                                    */
typedef struct ipx_ctx ipx_ctx_t;
/** Internal data structure that represents IPFIXcol record extension                          */
typedef struct ipx_ctx_ext ipx_ctx_ext_t;

#include <ipfixcol2/api.h>
#include <ipfixcol2/verbose.h>
#include <ipfixcol2/session.h>
#include <ipfixcol2/message.h>
#include <ipfixcol2/message_ipfix.h>

/**
 * \defgroup pluginAPI Plugin API
 * \ingroup publicAPIs
 * \brief Plugin interface and configuration functions
 *
 * @{
 */

/**
 * \defgroup pluginIFC Plugin Interface
 * \ingroup pluginAPI
 * \brief Functions to be implemented by a plugin.
 *
 * These functions specifies a communication interface between IPFIXcol core and external plugins.
 * The IPFIXcol core uses following functions to control processing messages by plugins. An author
 * of the plugin can count on the fact that for an instance of the plugin the functions are never
 * called concurrently. On the other hand, multiple instances of the same plugin could exists
 * inside the collector at the same time and run concurrently (by different threads), therefore,
 * an author of the plugin MUST avoid using non-constant global and static variables, system
 * signals and other resources that can potentially cause data race.
 *
 * This module contains the plugin identification structure and function that MUST be
 * implemented so a plugin can be loaded and used by the collector. Not all function mentioned
 * in this section must be implemented.
 *
 * Input plugins (::IPX_PT_INPUT) MUST implement at least these functions:
 * - plugin_init()
 * - plugin_destroy()
 * - plugin_get()
 * Optionally, if supported, the function plugin_session_close() should be implemented as well.
 *
 * Intermediate (::IPX_PT_INTERMEDIATE) and output (::IPX_PT_OUTPUT) plugins MUST implement at
 * least these functions:
 * - plugin_init()
 * - plugin_destroy()
 * - plugin_process()
 *
 * \note
 *   The plugin identification structure and all implemented functions MUST be defined as external
 *   symbols of the plugin's library (i.e. shared object) and MUST have public visibility.
 * @{
 */

/**
 * \def IPX_PT_INPUT
 * \brief Input plugin type
 *
 * Input plugins pass data to the IPFIXcol in a form of the IPFIX or NetFlow messages. The
 * source of data is completely independent and it's up to the plugin input to maintain
 * a connection with the source. Generally, we distinguish two kinds of sources - network and
 * file. Together with the messages also an information about the data source is passed.
 * Parsing of the message is provided by the IPFIXcol core.
 */
#define IPX_PT_INPUT 1U

/**
 * \def IPX_PT_INTERMEDIATE
 * \brief Intermediate plugin type
 *
 * Intermediate plugins get IPFIX messages and are allowed to modify, create and drop the
 * messages. The intermediate plugins are connected in series by ring buffers, therefore,
 * each message from an Input plugin goes through all plugins one by one, unless some plugin
 * discards it.
 */
#define IPX_PT_INTERMEDIATE 2U

/**
 * \def IPX_PT_OUTPUT
 * \brief Output plugin type
 *
 * Output plugins get IPFIX messages from the last intermediate plugin or directly from
 * input plugin, if no intermediate plugins are enabled. It's up to the output plugins what
 * they do with the messages. Records could be, for example, stored to a hard drive, forwarded
 * to another collector or analysis tool, converted to different data format, etc.
 * IPFIX messages MUST NOT be modified by the plugins.
 */
#define IPX_PT_OUTPUT 3U

/**
 * \def IPX_PF_DEEPBIND
 * \brief Use deep bind when resolving symbols of a plugin (and additional depending libraries)
 *
 * Some plugins might depend on an external library which redefines one or more common symbols
 * (e.g. thrd_create) used by the collector (or other plugins). Since common version of these
 * symbols is resolved before any plugin is loaded, these redefined symbols are ignored.
 * Therefore, the plugin (or third party libraries) might not be able to correctly work.
 *
 * This flag instructs the collector to use RTLD_DEEPBIND (see manual page of dlopen) which
 * solves this issue. However, it might not be supported by non-glibc implementations
 * (as it is a GNU extension) and might break some other functions. Use only if really required!
 */
#define IPX_PF_DEEPBIND 1U

/**
 * \brief Identification of a plugin
 *
 * This structure MUST be defined as global non-static variable called "ipx_plugin_info". In other
 * words, the variable MUST be an exported symbol of the plugin's library. Use IPX_API macro
 * to enable visibility of definition.
 */
struct ipx_plugin_info {
    /** Plugin identification name                                                            */
    const char *name;
    /** Brief description of the plugin                                                       */
    const char *dsc;
    /** Plugin type (one of #IPX_PT_INPUT, #IPX_PT_INTERMEDIATE, #IPX_PT_OUTPUT)              */
    uint16_t type;
    /** Configuration flags (zero or more IPX_PF_* values might be ORed here)                 */
    uint16_t flags;
    /** Plugin version string (like "1.2.3")                                                  */
    const char *version;
    /** Minimal IPFIXcol version string (like "1.2.3")                                        */
    const char *ipx_min;
};

/**
 * \brief Plugin instance initialization
 *
 * For each instance the function is called just once before any other interface function
 * is called by IPFIXcol core. During initialization the plugin should parsed user configuration,
 * prepare internal data structure for processing messages (if required) and store private instance
 * data to the context using ipx_ctx_private_set().
 *
 * \note
 *   Only successfully initialized plugins will be destroyed by plugin_destroy().
 * \note
 *   During initialization process, the internal pipeline connection between plugins has not been
 *   established yet and the plugin, therefore, cannot pass any messages.
 * \param[in] ctx    Plugin context
 * \param[in] params XML string with specific parameters for the plugin
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED if a memory allocation error has occurred or if the
 *   configuration \p params are not valid and the plugin is not initialized.
 */
IPX_API int
ipx_plugin_init(ipx_ctx_t *ctx, const char *params);

/**
 * \brief Plugin instance destruction
 *
 * For each instance the function is called just once as the last interface function call. During
 * destruction the plugin MUST return all resources and destroy its private instance data.
 *
 * \note
 *   It's still possible to pass messages to the internal pipeline. For example, input plugins
 *   MUST pass Transport Session messages (event type ::IPX_MSG_SESSION_CLOSE) to notify all
 *   remaining plugins that no more messages will be received from Transport Sessions which the
 *   plugin is maintaining.
 * \note Destruction should not fail.
 * \param[in] ctx Plugin context
 * \param[in] cfg Private data of the instance prepared by initialization function
 */
IPX_API void
ipx_plugin_destroy(ipx_ctx_t *ctx, void *cfg);

/**
 * \brief Get an IPFIX or NetFlow message from a data source (Input plugins ONLY)
 *
 * Each input plugin HAS to pass data to the IPFIXcol pipeline in the form of memory block
 * containing IPFIX or NetFlow message. It means that if an input plugin reads data in different
 * format than mentioned, the data must be transformed into the IPFIX (or NetFlow) message format.
 * Memory allocated by the input plugin for the messages data is freed later by IPFIXcol core
 * automatically. The messages must be wrapped inside \ref ipxMsgIPFIX "IPFIX Messages" and
 * passed using ipx_ctx_msg_pass(). At most one "data" message should be passed during
 * execution of this function.
 *
 * Together with the data, input plugin further passes information about Transport Session,
 * Observation Domain ID and Stream ID. The plugin can count on the fact that this information data
 * are read only for other plugins that works with messages and are not changed by IPFIXcol core.
 *
 * Moreover, the first message with the data from each Transport Session MUST precede a Transport
 * Session message (event type ::IPX_MSG_SESSION_OPEN) that holds information about new connection.
 * After termination of each Transport Session (i.e. no more data will be received from a specific
 * Transport Session) the plugin MUST send another Transport Session message (event type
 * ::IPX_MSG_SESSION_CLOSE) to inform all plugins that the Session has been closed.
 *
 * \warning
 *   This interface is only for Input plugins! In case of the other types, the IPFIXcol core
 *   ignores this function.
 * \param[in] ctx Plugin context
 * \param[in] cfg Private data of the instance prepared by initialization function
 * \return #IPX_OK on success (or if a non-fatal error has occurred and the plugin can continue to
 *   work)
 * \return #IPX_ERR_DENIED if a fatal memory allocation error has occurred and/or the plugin cannot
 *   continue to work properly (the plugin will be destroyed immediately).
 * \return #IPX_ERR_EOF if the end of file/stream has been reached and the plugin cannot provide
 *   more data from any sources. The plugin will be destroyed immediately and if this is also the
 *   last running input plugin, the collector will exit.
 */
IPX_API int
ipx_plugin_get(ipx_ctx_t *ctx, void *cfg);

/**
 * \brief Process a message from the IPFIXcol core (Intermediate and Output plugins ONLY)
 *
 * This function is called for each message (from a pipeline predecessor) the instance subscribes
 * to. The way of data processing is completely up to the specific plugin.
 *
 * In case of _Intermediate plugins_, the function can modify, for example, IPFIX messages (change
 * value of record fields, add or remove fields, etc.). After processing each message the plugin
 * should pass the message to a successor plugin (a next intermediate plugin in the pipeline or
 * output plugins) using ipx_ctx_msg_pass(). It's not done automatically because any message can
 * be even dropped, but author of the plugin MUST be sure that there are not other references
 * to data.
 *
 * In case of _Output plugins_, the function MUST NOT modify received messages because they can
 * be concurrently used by other plugins. Read the previous sentence again, slowly, one more time
 * please. The basic usage of Output plugins is to store or forward all data in a specific format,
 * but also various processing (statistics, etc.) can be done.
 *
 * \warning
 *   This interface is only for Intermediate and Output plugins! In case of the other types,
 *   the IPFIXcol core ignores this function.
 * \note
 *   By default, the plugin receives only IPFIX Messages. To change what kind of messages the
 *   plugin wants to receive use ipx_ctx_subscribe().
 * \param[in] ctx Plugin context
 * \param[in] cfg Private data of the instance prepared by initialization function
 * \param[in] msg Message to process
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED if a fatal memory allocation error has occurred and/or the plugin cannot
 *   continue to work properly (the collector will exit).
 * \return #IPX_ERR_EOF if the plugin has reached expected goal (e.g. number of processed records).
 *   This function will not be called anymore and the collector will shut down.
 */
IPX_API int
ipx_plugin_process(ipx_ctx_t *ctx, void *cfg, ipx_msg_t *msg);

/**
 * \brief Request to close a Transport Session (Input plugins only!)
 *
 * If possible, all input plugins should implement this function. It can be used by the IPFIXcol
 * core to close a Transport Session if a malformed message has been received to restart the
 * Session. If the input cannot close the Transport Session (e.g. UDP sessions, etc.), this
 * function should not be implemented at all.
 *
 * After _successful_ closing of the session, the plugin MUST create and pass Session status
 * message with event type ::IPX_MSG_SESSION_CLOSE.
 *
 * \warning
 *   This interface is only for Input plugins! In case of the other types, the IPFIXcol core
 *   ignores this function.
 * \warning
 *   Do NOT access Session information properties because the structure can be already freed,
 *   if the plugin removed it before receiving the request! Use can use ONLY pointer to the
 *   Session structure to compare it with known sessions!
 * \param[in] ctx     Plugin context
 * \param[in] cfg     Private data of the instance prepared by initialization function
 * \param[in] session Pointer to the Transport Session to close
 */
IPX_API void
ipx_plugin_session_close(ipx_ctx_t *ctx, void *cfg, const struct ipx_session *session);

/**
 * @}
 *
 * \defgroup ContextAPI Context API
 * \ingroup pluginAPI
 * \brief Instance configuration and contextual data
 *
 * @{
 */

/**
 * \brief Set private data of the instance
 *
 * Private data is used to distinguish individual instance of the same plugin and is passed by
 * the IPFIXcol to the plugin function every time it is called. A form of data is always up to
 * the plugin, however, usually it is represented as a plugin specific structure.
 *
 * \note By default, private data of the instance is NULL.
 * \param[in] ctx  Current plugin context
 * \param[in] data Pointer to private data (can be NULL).
 */
IPX_API void
ipx_ctx_private_set(ipx_ctx_t *ctx, void *data);

/**
 * \brief Get verbosity level of the instance
 * \param[in] ctx Current plugin context
 * \return Verbosity level
 */
IPX_API enum ipx_verb_level
ipx_ctx_verb_get(const ipx_ctx_t *ctx);

/**
 * \brief Get name of the instance (as mention in the runtime configuration)
 * \param[in] ctx Current plugin context
 * \return Name
 */
IPX_API const char *
ipx_ctx_name_get(const ipx_ctx_t *ctx);

/**
 * \brief Pass a message to a successor of the plugin (only Input and Intermediate plugins ONLY!)
 *
 * The message is pushed into an output queue of the instance and will be later processed by
 * a successor. When the message does not fit into the queue of messages, the function blocks.
 * The message can be of any type supported by the collector. Therefore, the plugin can use it
 * to pass processed IPFIX messages, information about Transport Sessions, garbage that cannot
 * be freed because someone is still using it, etc.
 *
 * During plugin instance initialization, messages cannot be passed because connection between
 * plugins haven't been established yet.
 *
 * \warning
 *   This interface is only for Input and Intermediate plugins.
 * \param[in] ctx Current plugin context
 * \param[in] msg Message to send
 * \return #IPX_OK on success.
 * \return #IPX_ERR_ARG if the \p msg is NULL, the plugin doesn't have permissions.
 */
IPX_API int
ipx_ctx_msg_pass(ipx_ctx_t *ctx, ipx_msg_t *msg);

/**
 * \brief Change message subscription (Intermediate and Output plugins ONLY!)
 *
 * Each instance can define types of messages that are passed into ipx_plugin_process().
 * \p mask_new and \p mask_old specifies a set of message types as bitwise OR of zero or more
 * of the flags defined by ::ipx_msg_type. Usually plugin can only subscribe to the following
 * types of messages:
 * - ::IPX_MSG_IPFIX (IPFIX Message)
 * - ::IPX_MSG_SESSION (Transport Session Message)
 *
 * If \p mask_new is non-NULL, the new subscription mask is installed from \p mask_new.
 * If \p mask_old is non-NULL, the previous mask is saved in \p mask_old.
 *
 * \note By default, each plugin is subscribed only to receive IPFIX Messages.
 * \note In case of Intermediate plugins, message types that are not subscribed are automatically
 *   passed to the successor of the instance!
 * \warning
 *   This interface is only for Intermediate and Output plugins!
 * \param[in]  ctx      Plugin context
 * \param[in]  mask_new New message mask (can be NULL)
 * \param[out] mask_old Old message mask (can be NULL)
 * \return #IPX_OK on success
 * \return #IPX_ERR_FORMAT if the \p mask_new contains unknown message type or the instance doesn't
 *   have permission to subscribe these types of messages (the new mask is not installed)
 * \return #IPX_ERR_ARG if the plugin is not of proper type (i.e. not Intermediate or Output)
 */
IPX_API int
ipx_ctx_subscribe(ipx_ctx_t *ctx, const ipx_msg_mask_t *mask_new, ipx_msg_mask_t *mask_old);

/**
 * \brief Get a manager of Information Elements
 *
 * \warning
 *   It IS recommended to avoid storing references to definitions of the manager, because the
 *   manager can be updated during reconfiguration. If you need to store a reference to the manager,
 *   you MUST implement update function and react to IE manager modifications!
 * \param[in] ctx Plugin context
 * \return Pointer to the manager (never NULL)
 */
IPX_API const fds_iemgr_t *
ipx_ctx_iemgr_get(ipx_ctx_t *ctx);

/**
 * \brief Register an extension of Data Records (Intermediate plugins ONLY!)
 *
 * Reserve space for metadata that will be part of each Data Record. The purpose of extension
 * it is to add non-flow information which can be useful during record processing. For example,
 * one plugin can add some labels and one or more plugins further in the pipeline can use them
 * later.
 *
 * Structure or data type of the extension is up to the producer. Nevertheless, the producer and
 * all consumers must use the same. The producer is also RESPONSIBLE for filling content of the
 * extension to EACH Data Record in the IPFIX Message! After filling the extension, function
 * ipx_ctx_ext_set_filled() must be called to mark the extension memory as filled. Otherwise,
 * consumers are not able get its content.
 *
 * One plugin instance can register multiple extensions.
 * \warning
 *   This function can be called only during ipx_plugin_init() of Intermediate plugins.
 * \note
 *   Only single plugin instance at time can produce extension with the given combination
 *   of the \p type and the \p name.
 * \param[in]  ctx  Plugin context
 * \param[in]  type Identification of the extension type (e.g. "profiles-v1")
 * \param[in]  name Identification of the extension name (e.g. "main_profiles")
 * \param[in]  size Non-zero size of memory required for the extension (in bytes)
 * \param[out] ext  Internal description of the extension
 *
 * \return #IPX_OK on success
 * \return #IPX_ERR_ARG if the \p type or \p name are not valid (i.e. empty or NULL) or \p size
 *   is zero.
 * \return #IPX_ERR_DENIED if the plugin doesn't have permission to register extension
 * \return #IPX_ERR_EXISTS if the extension or dependency has been already registered by this plugin
 * \return #IPX_ERR_NOMEM if a memory allocation failure has occurred
 */
IPX_API int
ipx_ctx_ext_producer(ipx_ctx_t *ctx, const char *type, const char *name, size_t size,
    ipx_ctx_ext_t **ext);

/**
 * @brief Add dependency on an extension of Data Records (Intermediate and Output plugins ONLY!)
 *
 * Register dependency on an extension. This will make sure that the required extension is
 * available for EACH Data Record during ipx_plugin_process() and that there is a particular
 * producer earlier in the processing pipeline.
 *
 * One plugin instance can register multiple dependencies.
 *
 * \warning
 *   This function can be called only during ipx_plugin_init() of Intermediate and Output plugins.
 * \note
 *   If the function has succeeded, it doesn't mean that there is particular extension producer.
 *   Since dependencies are resolved later during configuration of the collector, startup
 *   process will be interrupted if all requirements are not met.
 * \note
 *   The plugin instance CANNOT add dependency on an extension which it is producing.
 *
 * \param[in]  ctx  Plugin context
 * \param[in]  type Identification of the extension type (e.g. "profiles-v1")
 * \param[in]  name Identification of the extension name (e.g. "main_profiles")
 * \param[out] ext  Internal description of the extension
 *
 * \return #IPX_OK on success (see notes)
 * \return #IPX_ERR_ARG if the \p type or \p name are not valid (i.e. empty or NULL)
 * \return #IPX_ERR_DENIED if the plugin doesn't have permission to register dependency
 * \return #IPX_ERR_EXISTS if the dependency or extension has been already registered by this plugin
 * \return #IPX_ERR_NOMEM if a memory allocation failure has occurred
 */
IPX_API int
ipx_ctx_ext_consumer(ipx_ctx_t *ctx, const char *type, const char *name, ipx_ctx_ext_t **ext);

/**
 * \brief Get an extension
 *
 * In case of a producer of the extension, it always returns #IPX_OK and fills the pointer and
 * size. If the producer decides to fill the extension, it also must call ipx_ctx_ext_set_filled().
 * Otherwise, consumers will not be able to get its content.
 *
 * \param[in]  ext  Internal description of the extension
 * \param[in]  drec Data Record with extensions
 * \param[out] data Pointer to extension data
 * \param[out] size Size of the extensions (bytes)
 * \return #IPX_OK on success
 * \return #IPX_ERR_NOTFOUND if the extension hasn't been filled by its producer
 */
IPX_API int
ipx_ctx_ext_get(ipx_ctx_ext_t *ext, struct ipx_ipfix_record *drec, void **data, size_t *size);

/**
 * \brief Set the extension of a Data Record as filled (ONLY for the producer of the extension)
 *
 * \param[in] ext  Internal description of the extension
 * \param[in] drec Data Record with extensions
 */
IPX_API void
ipx_ctx_ext_set_filled(ipx_ctx_ext_t *ext, struct ipx_ipfix_record *drec);

/**
 * @}
 * @}
 */

#ifdef __cplusplus
}
#endif
#endif // IPX_PLUGINS_H
