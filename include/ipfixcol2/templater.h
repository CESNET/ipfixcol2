/**
 * \file   include/ipfixcol2/templater.h
 * \author Michal Režňák
 * \brief  Simple template manager
 * \date   17. August 2017
 */

/* Copyright (C) 2016 CESNET, z.s.p.o.
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

#ifndef TEMPLATER_TMPLMGR_H
#define TEMPLATER_TMPLMGR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <libfds/iemgr.h>
#include <ipfixcol2/api.h>
#include "message_garbage.h"
#include "ipfix_structures.h"
#include "source.h"

/** Types of IPFIX (Options) Templates                                  */
enum IPX_TEMPLATE_TYPE {
    IPX_TEMPLATE,         /**< Definition of Template                   */
    IPX_TEMPLATE_OPTIONS, /**< Definition of Options Template           */
};

/**
 * \brief Standard types of Options Templates
 * \remark Based on RFC 7011, Section 4.
 */
enum IPX_OPTS_TEMPLATE_TYPE {
    IPX_OPTS_NO_OPTIONS,                   /**< Not an Options Template i.e. "Normal" Template */
    IPX_OPTS_METER_PROC_STAT,              /**< The Metering Process Statistics                */
    IPX_OPTS_METER_PROC_RELIABILITY_STAT,  /**< The Metering Process Reliability Statistics    */
    IPX_OPTS_EXPORT_PROC_RELIABILITY_STAT, /**< The Exporting Process Reliability Statistics   */
    IPX_OPTS_FLOW_KEYS,                    /**< The Flow Keys                                  */
    IPX_OPTS_UNKNOWN                       /**< Unknown type of Options Template               */
}; // uint32 bit field

/**
 * \brief Structure for parsed IPFIX element in an IPFIX template
 */
struct ipx_tmpl_template_field {
    /** Enterprise Number      */
    uint32_t en;
    /** Information Element ID */
    uint16_t id;

    /**
     * The real length of the Information Element.
     * The value #IPFIX_VAR_IE_LENGTH (i.e. 65535) is reserved for
     * variable-length information elements.
     */
    uint16_t length;
    /**
     * The offset from the start of a data record in octets.
     * The value #IPFIX_VAR_IE_LENGTH (i.e. 65535) is reserved for unknown
     * offset when there is at least one variable-length element among
     * preceding elements in the same template.
     */
    uint16_t offset;
    /**
     * The last of identical Elements with the same combination of an
     * Information Element ID and an Enterprise Number, i.e. if equals to
     * "false", then there is at least one more element with the same
     * combination and a higher index in the template.
     */
    bool last_identical;

    /**
     * Detailed definition of the element (data/semantic/unit type).
     * Can be NULL, when the definition is missing in the configuration.
     */
    const struct fds_iemgr_elem* definition;
};

/** Templater */
typedef struct ipx_tmpl ipx_tmpl_t;
/** Template */
typedef struct ipx_tmpl_template ipx_tmpl_template_t;

/**
 * \brief Create templater
 * \param[in] life_time   Time after which template is old (UDP only)
 * \param[in] life_packet Number of IPFIX messages after which template is old (UDP only)
 * \param[in] type        Protocol of the session
 * \return Templater on success, otherwise NULL when memory error
 */
IPX_API ipx_tmpl_t*
ipx_tmpl_create(uint64_t life_time, uint64_t life_packet, enum ipx_session_type type);

/**
 * \brief Load iemgr to the templater and set elements from a iemgr to fields in templates
 *
 * Every templater needs a iemgr to know which template field belongs to which informational element.
 * For example when ipx_tmpl_template_add is called, then it needs iemgr to know which
 * informational element belongs to the template field's element definition.
 * When templater doesn't have any iemgr, than other functions will not work correctly.
 * \param[in,out] tmpl Templater
 * \param[in]     mgr  Manager
 * \return IPX_OK when manager is saved correctly, otherwise IPX_ERR or IPX_NOMEM
 *
 * \note When templater already contain some manager, than it will be overwritten
 * \warning Iemgr must exist as long as templater exist.
 * \warning Templater will not make a copy of iemgr.
 */
IPX_API int
ipx_tmpl_iemgr_load(ipx_tmpl_t* tmpl, struct fds_iemgr* mgr);

/**
 * \brief Destroy templater
 * \param[in] tmpl Templater
 */
IPX_API void
ipx_tmpl_destroy(ipx_tmpl_t* tmpl);

/**
 * \brief Set current time and number of IPFIX message
 * \param[in,out] tmpl          Templater
 * \param[in]     current_time  Current time
 * \param[in]     current_packet Number of the last IPFIX message
 * \return IPX_OK on success, otherwise IPX_ERR
 *
 * \note
 * May occur a situation when newer IPFIX message is sent earlier than older message. That's why
 * current time must be set. Templater will compare \p current \p time with template's time
 * and 'do something with' template only if \p current \p time is between template's \p first
 * and \p last time (see ipx_template_time).
 * \note
 * When UDP protocol is used, than template's life can end after a number of IPFIX messages.
 * More information <a href="https://tools.ietf.org/html/rfc6728#section-4.5.2">here</a>
 * \note current_time should be set every time when IPFIX message is send.
 * \note Current count is important only for UDP protocol, in others is ignored (can be set to 0). TODO
 */
IPX_API void
ipx_tmpl_set(ipx_tmpl_t* tmpl, uint64_t current_time, uint64_t current_packet);

/**
 * \brief Remove all templates from a templater (withdrawal all)
 *
 * When UDP protocol is set, then remove all templates.
 * When TCP or SCTP protocol is set, then only \p last time is set to the \p compare \p time
 * (see ipx_template_time and ipx_tmpl_set).
 * Thus ipx_tmpl_set should be called before this.
 * It doesn't remove templates because when older message will be sent, than it need older
 * template to read that message properly.
 * \param[in] tmpl Templater
 * \return IPX_OK when all templates are removed, otherwise IPX_ERR
 *
 * \warning \p ipx_tmpl_set should be called before to work properly
 * \note All other parameters are untouched.
 */
IPX_API int
ipx_tmpl_clear(ipx_tmpl_t* tmpl);

/**
 * \brief Remove template from a templater (withdrawal)
 *
 * When UDP protocol is set, then remove all templates with defined ID.
 * When TCP or SCTP protocol is set, then only \p last time is set to the \p compare \p time
 * (see ipx_template_time and ipx_tmpl_set).
 * Thus ipx_tmpl_set should be called before this.
 * It doesn't remove template because when older message will be sent, than it need older
 * template to read that message properly.
 * \param[in,out] tmpl Templater
 * \param[in]     id   ID of a template
 * \return IPX_OK when template is removed, or IPX_NOT_FOUND when template with defined ID
 * doesn't exists, otherwise IPX_ERR or IPX_NOMEM
 *
 * \warning \p ipx_tmpl_set should be called before to work properly
 * \note UDP cannot send withdrawal scopes thus when sent all templates with that ID will be on
 * next get_garbage call removed
 */
IPX_API int
ipx_tmpl_template_remove(ipx_tmpl_t* tmpl, uint16_t id);

/**
 * \brief Parse template set
 *
 * Parse template set, than save template to the templater or do other action which depends
 * on the protocol, templates length etc. More information in notes below.
 * \param[in,out] tmpl Templater
 * \param[in]     head Head of the template set
 * \return IPX_OK when template set was saved successfully, otherwise IPX_ERR or IPX_NOMEM
 *
 * \warning \p ipx_tmpl_set should be called before to work properly
 * \note It is recommended to use garbage_get after every call
 * \note
 * When UDP protocol is defined withdrawal cannot be used, but all elements can be overwritten.
 * When TCP protocol is defined withdrawal can be sent, but element cannot be overwritten.
 * Only exact same elements can be sent.
 * When SCTP protocol is defined withdrawal can be sent, but element cannot be overwritten.
 * Only exact same elements can be sent.
 * \note
 * If template has length zero, than instead of saving the template will be removed (withdrawal)
 * if protocol supports this feature.
 * This behavior is defined <a href="https://tools.ietf.org/html/rfc7011#section-8.1">here</a>.
 * \note UDP cannot send withdrawal scopes thus when sent all templates with that ID will be on next
 * get_garbage call removed */
IPX_API int
ipx_tmpl_template_set_parse(ipx_tmpl_t* tmpl, struct ipfix_set_header* head);

/**
 * \brief Parse template to the context
 *
 * Save template to the templater or do other action which depends
 * on the protocol, templates length etc. More information in notes below.
 * \param[in,out] tmpl    Templater
 * \param[in]     rec     Template record
 * \param[in]     max_len Maximal length of the template
 * \return The real length in bytes of the parsed template when saved successfully,
 * otherwise IPX_ERR or IPX_NOMEM.
 *
 * \warning \p ipx_tmpl_set should be called before to work properly
 * \note It is recommended to use garbage_get after every call
 * \note IPX_OK cannot be returned.
 * \note
 * When UDP protocol is defined withdrawal cannot be used, but all elements can be overwritten.
 * When TCP protocol is defined withdrawal can be sent, but element cannot be overwritten.
 * Only exact same elements can be sent.
 * When SCTP protocol is defined withdrawal can be sent, but element cannot be overwritten.
 * Only exact same elements can be sent.
 * \note
 * If template has length zero, than instead of defining the template will be removed (withdrawal)
 * if protocol supports this feature.
 * This behavior is defined <a href="https://tools.ietf.org/html/rfc7011#section-8.1">here</a>.
 * \note UDP cannot send withdrawal scopes thus when sent all templates with that ID will be on next
 * get_garbage call removed
 * \note In a maximal length is also counted size of the template head thus cannot be smaller
 * than TEMPL_HEAD_SIZE.
 */
IPX_API int
ipx_tmpl_template_parse(ipx_tmpl_t* tmpl, const struct ipfix_template_record* rec, uint16_t max_len);

/**
 * \brief Parse options template to the context
 *
 * Save template to the templater or do other action which depends
 * on the protocol, templates length etc. More information in notes below.
 * \param[in,out] tmpl    Templater
 * \param[in]     rec     Options template record
 * \param[in]     max_len Maximal length of the template
 * \return The real length in bytes of the parsed template when saved successfully,
 * otherwise IPX_ERR or IPX_NOMEM
 *
 * \warning \p ipx_tmpl_set should be called before to work properly
 * \note it is recommended to use garbage_get after every call
 * \note
 * When UDP protocol is defined withdrawal cannot be used, but all elements can be overwritten.
 * When TCP protocol is defined withdrawal can be sent, but element cannot be overwritten.
 * Only exact same elements can be sent.
 * When SCTP protocol is defined withdrawal can be sent, but element cannot be overwritten.
 * Only exact same elements can be sent.
 * \note
 * If options template has length zero, than instead of defining the template
 * will be removed (withdrawal) if protocol supports this feature.
 * This behavior is defined <a href="https://tools.ietf.org/html/rfc7011#section-8.1">here</a>.
 * \note UDP cannot send withdrawal scopes thus when sent all templates with that ID will be on next
 * get_garbage call removed
 */
IPX_API int
ipx_tmpl_options_template_parse(ipx_tmpl_t* tmpl, const struct ipfix_options_template_record* rec,
                                uint16_t max_len);

/**
 * \brief Find template wit defined ID in a templater
 * \param[in]  tmpl     Templater
 * \param[in]  id       ID of a searched template
 * \param[out] dst      Founded template
 * \return IPX_OK when template with defined ID was founded, otherwise IPX_ERR.
 *
 * \warning \p ipx_tmpl_set should be called before to work properly
 * \warning UDP protocol can also return IPX_OK_OLD, if different between current_count
 * (see ipx_tmpl_set) and template's count (see struct ipx_tmpl) is bigger
 * than template's life_count (see ipx_tmpl_create).
 * More information <a href="https://tools.ietf.org/html/rfc6728#section-4.5.2">here</a>
 * where life_count is templateLifePacket.
 * \note Also create a snapshot (see ipx_tmpl_snapshot_get)
 */
IPX_API int
ipx_tmpl_template_get(const ipx_tmpl_t *tmpl, uint16_t id, ipx_tmpl_template_t **dst);

/**
 * \brief Create snapshot from a templater with a current state
 * \param[in] tmpl Templater
 * \return Snapshot in a form of a context if exists, otherwise NULL
 *
 * \warning \p ipx_tmpl_set should be called before to work properly
 * \note Snapshot is only a templater, which cannot be changed, in one exact time.
 * So all functions which take 'const templater' can be called over a snapshot.
 */
IPX_API const ipx_tmpl_t *
ipx_tmpl_snapshot_get(ipx_tmpl_t *tmpl);

/**
 * \brief From a templater return garbage, which is all objects that are only in deprecated snapshots
 *
 * When overwriting some template, previous template is not destroyed. It's only moved to deprecated.
 * Thus once per time this function must be called to remove al deprecated objects.
 *
 * Function wrap all garbage to the garbage_message, which can be later called to destroy garbage.
 * \param[in] tmpl Templater
 * \return Message with garbage if exist, otherwise NULL
 *
 * \warning \p ipx_tmpl_set should be called before to work properly
 */
IPX_API ipx_garbage_msg_t *
ipx_tmpl_garbage_get(ipx_tmpl_t* tmpl);

/**
 * \brief Get the description of a field with a given index in a template
 * \param[in] src   Template
 * \param[in] index Index of the field
 * \return Field with defined Index on success, otherwise NULL
 */
IPX_API const struct ipx_tmpl_template_field*
ipx_tmpl_template_field_get(ipx_tmpl_template_t* src, size_t index);

/**
 * \brief Get template type
 * \param[in] src Template
 * \return Template type
 */
IPX_API enum IPX_TEMPLATE_TYPE
ipx_tmpl_template_type_get(const ipx_tmpl_template_t* src);

/**
 * \brief Get option template type
 * \param[in] src Template
 * \return Option template type
 */
IPX_API enum IPX_OPTS_TEMPLATE_TYPE
ipx_tmpl_template_opts_type_get(const ipx_tmpl_template_t* src);

/**
 * \brief Get template ID
 * \param[in] src Template
 * \return ID of the template
 */
IPX_API uint16_t
ipx_tmpl_template_id_get(const ipx_tmpl_template_t* src);

#ifdef __cplusplus
}
#endif

#endif //TEMPLATER_TMPLMGR_H
