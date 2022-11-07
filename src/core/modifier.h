/**
 * \file modifier.h
 * \author Adam Zvara <xzvara01@stud.fit.vutbr.cz>
 * \brief Modifier component (internal header file)
 * \date 2023
 */

#ifndef IPX_MODIFIER_INTERNAL_H
#define IPX_MODIFIER_INTERNAL_H

#include <inttypes.h>
#include <ipfixcol2.h>

#include "verbose.h"

/** Default amount of contexts used for transport sessions  */
#define IPX_MODIFIER_DEF_CTX 1u

/**
 * \brief Modifier context (template manager and last used template ID)
 *  identified by session and ODID
 */
struct session_odid_ctx {
    /** Transport Session                       */
    const struct ipx_session *session;
    /** Observation Domain ID                   */
    uint32_t odid;

    /** Template manager for given session      */
    struct fds_tmgr *mgr;
    /** ID of last added template               */
    uint16_t next_id;
};

/** Auxiliary structure for garbage             */
struct session_garbage {
    /** Number of records                       */
    size_t rec_cnt;
    /** Array of mangers to destroy             */
    struct fds_tmgr **mgrs;
};

/**
 * \brief Modifier component used for adding or filtering out
 *  elements from ipfix message
 */
struct ipx_modifier {
    /** String identifying module using modifier                */
    char *ident;
    /** Verbosity level                                         */
    enum ipx_verb_level vlevel;
    /** Information about new elements                          */
    const struct ipx_modifier_field *fields;
    /** Fields count                                            */
    size_t fields_cnt;
    /** Callback for adding new elements                        */
    modifier_adder_cb_t cb_adder;
    /** Callback for filtering elements                         */
    modifier_filter_cb_t cb_filter;
    /** Shared callback data                                    */
    void *cb_data;

    /** Element manager                                         */
    const fds_iemgr_t *iemgr;
    /** Current transport session context                       */
    struct session_odid_ctx *curr_ctx;

    struct {
        /** Number of pre-allocated session ctx records         */
        size_t ctx_alloc;
        /** Number of valid session ctx records                 */
        size_t ctx_valid;
        /** Currently opened transport streams + odid contexts  */
        struct session_odid_ctx *ctx;
    } sessions;
};

/**
 * \def MODIFIER_ERROR
 * \brief Macro for printing an error message of a modifier
 * \param[in] mod     Modifier
 * \param[in] fmt     Format string (see manual page for "printf" family)
 * \param[in] ...     Variable number of arguments for the format string
 */
#define MODIFIER_ERROR(mod, fmt, ...)                                           \
    if ((mod)->vlevel >= IPX_VERB_ERROR) {                                      \
        ipx_verb_print(IPX_VERB_ERROR, "ERROR: %s: " fmt "\n",                  \
            (mod)->ident, ## __VA_ARGS__);                                      \
    }

/**
 * \def MODIFIER_WARNING
 * \brief Macro for printing a warning message of a modifier
 * \param[in] mod     Modifier
 * \param[in] fmt     Format string (see manual page for "printf" family)
 * \param[in] ...     Variable number of arguments for the format string
 */
#define MODIFIER_WARNING(mod, fmt, ...)                                         \
    if ((mod)->vlevel >= IPX_VERB_WARNING) {                                    \
        ipx_verb_print(IPX_VERB_ERROR, "WARNING: %s: " fmt "\n",                \
            (mod)->ident, ## __VA_ARGS__);                                      \
    }

/**
 * \def MODIFIER_INFO
 * \brief Macro for printing an info message of modifier
 * \param[in] mod     Modifier
 * \param[in] fmt     Format string (see manual page for "printf" family)
 * \param[in] ...     Variable number of arguments for the format string
 */
#define MODIFIER_INFO(mod, fmt, ...)                                            \
    if ((mod)->vlevel >= IPX_VERB_INFO) {                                       \
        ipx_verb_print(IPX_VERB_INFO, "INFO: Modifier (%s): " fmt "\n",         \
            (mod)->ident, ## __VA_ARGS__);                                      \
    }

/**
 * \def MODIFIER_DEBUG
 * \brief Macro for printing a debug message of modifier
 * \param[in] mod     Modifier
 * \param[in] fmt     Format string (see manual page for "printf" family)
 * \param[in] ...     Variable number of arguments for the format string
 */
#define MODIFIER_DEBUG(mod, fmt, ...)                                           \
    if ((mod)->vlevel >= IPX_VERB_DEBUG) {                                      \
        ipx_verb_print(IPX_VERB_INFO, "DEBUG: Modifier (%s): " fmt "\n",        \
            (mod)->ident, ## __VA_ARGS__);                                      \
    }

#endif