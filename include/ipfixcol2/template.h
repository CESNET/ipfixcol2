/**
 * \file   include/ipfixcol2/template.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief  Template (header file)
 * \date   October 2017
 */
/*
 * Copyright (C) 2017 CESNET, z.s.p.o.
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

#ifndef IPFIXCOL_TEMPLATE_H
#define IPFIXCOL_TEMPLATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <libfds/iemgr.h>
#include <ipfixcol2/api.h>

/** Unsigned integer type able to hold all template flags */
typedef uint16_t ipx_template_flag_t;

/** \brief Template Field features                                                               */
enum ipx_tfield_features {
    /**
     * \brief Scope field
     *
     * If this state flag is set, this is a scope field.
     */
    IPX_TFIELD_SCOPE = (1 << 0),
        /**
     * \brief Multiple occurrences of this Information Element (IE)
     *
     * If this flag is set, there are multiple occurrences of this IE anywhere in the template
     * to which the field belongs.
     */
    IPX_TFIELD_MULTI_IE = (1 << 1),
    /**
     * \brief The last occurrence of this Information Elements (IE)
     *
     * If this flags is set, there are NO more occurrences of the IE with the same combination of
     * an Information Element ID and an Enterprise Number in a template to which the field belongs.
     * In other words, if this flag is NOT set, there is at least one IE with the same definition
     * and with _higher_ index in the template.
     * \note This flag is also set if there are NOT multiple occurrences of the same IE.
     */
    IPX_TFIELD_LAST_IE = (1 << 2),
    /**
     * \brief Is it a field of structured data
     *
     * In other words, if this flag is set, the field is the basicList, subTemplateList, or
     * subTemplateMultiList Information Element (see RFC 6313).
     * \note To distinguish whether the IE is structured or not, an external database of IEs must
     *   be used. For example, use the IE manager distributed with libfds. In other words, this
     *   information is not a part of a template definition. See ipx_template_define_ies() for
     *   more information.
     */
    IPX_TFIELD_STRUCTURED = (1 << 3),
    /**
     * \brief Reverse Information Element
     *
     * An Information Element defined as corresponding to a normal (or forward) Information
     * Element, but associated with the reverse direction of a Biflow.
     * \note To distinguish whether the IE is reverse or not, an external database of IEs must be
     *   used. For example, use the IE manager distributed with libfds. In other words, this
     *   information is not a part of a template definition. See ipx_template_define_ies() for
     *   more information.
     */
    IPX_TFIELD_REVERSE = (1 << 4),
    /**
     * \brief Flow key Information Element
     *
     * \note To distinguish whether the IE is a flow key or not, an exporter must send a special
     *   record. In other words, this information is not a part of a template definition. See
     *   ipx_template_define_flowkey() for more information.
     */
    IPX_TFIELD_FLOW_KEY = (1 << 5)
};

/** \brief Structure of a parsed IPFIX element in an IPFIX template                              */
struct ipx_tfield {
    /** Enterprise Number      */
    uint32_t en;
    /** Information Element ID */
    uint16_t id;

    /**
     * The real length of the Information Element.
     * The value #IPFIX_VAR_IE_LENGTH (i.e. 65535) is reserved for variable-length information
     *   elements.
     */
    uint16_t length;
    /**
     * The offset from the start of a data record in octets.
     * The value #IPFIX_VAR_IE_LENGTH (i.e. 65535) is reserved for unknown offset if there is
     * at least one variable-length element among preceding elements in the same template.
     */
    uint16_t offset;
    /**
     * Features specific for this field.
     * The flags argument contains a bitwise OR of zero or more of the flags defined in
     * #ipx_tfield_features enumeration.
     */
    ipx_template_flag_t flags;
    /**
     * Detailed definition of the element (data/semantic/unit type).
     * If the definition is missing in the configuration, the values is NULL.
     */
    const struct fds_iemgr_elem *def;
};

/** Types of IPFIX (Options) Templates                                                           */
enum ipx_template_type {
    IPX_TYPE_TEMPLATE,       /**< Definition of Template                                         */
    IPX_TYPE_TEMPLATE_OPTS,  /**< Definition of Options Template                                 */
    IPX_TYPE_TEMPLATE_UNDEF  /**< For internal usage                                             */
};

/**
 * \brief Types of Options Templates
 *
 * These types of Option Templates are automatically recognized by template parser. Keep in mind
 * that multiple types can be detected at the same time.
 * \remark Standard types comes are based on RFC 7011, Section 4.
 */
enum ipx_template_opts_type {
    /** The Metering Process Statistics Options Template                                         */
    IPX_OPTS_MPROC_STAT = (1 << 0),
    /** The Metering Process Reliability Statistics Options Template                             */
    IPX_OPTS_MPROC_RELIABILITY_STAT = (1 << 1),
    /** The Exporting Process Reliability Statistics Options Template                            */
    IPX_OPTS_EPROC_RELIABILITY_STAT = (1 << 2),
    /** The Flow Keys Options Template                                                           */
    IPX_OPTS_FKEYS = (1 << 3),
    /** The Information Element Type Options Template (RFC 5610)                                 */
    IPX_OPTS_IE_TYPE = (1 << 4)
    // Add new ones here...
};

/** \brief Template features                                                                     */
enum ipx_template_features {
    /** Template has multiple occurrences of the same Information Element (IE) in the template.  */
    IPX_TEMPLATE_HAS_MULTI_IE = (1 << 0),
    /** Template has at least one Information Element of variable length                         */
    IPX_TEMPLATE_HAS_DYNAMIC = (1 << 1),
    /** Is it a Biflow template (has at least one Reverse Information Element)                   */
    IPX_TEMPLATE_HAS_REVERSE = (1 << 2),
    /** Template has at least one structured data type (basicList, subTemplateList, etc.)        */
    IPX_TEMPLATE_HAS_STRUCT = (1 << 3),
    /** Template has a known flow key (at least one field is marked as a Flow Key)               */
    IPX_TEMPLATE_HAS_FKEY = (1 << 4),
    // Add new ones here...
};

/**
 * \brief Structure for a parsed IPFIX template
 *
 * This dynamically sized structure wraps a parsed copy of an IPFIX template.
 * \warning Never modify values directly. Otherwise, consistency of the template cannot be
 *   guaranteed!
 */
struct ipx_template {
    /** Type of the template                                                                     */
    enum ipx_template_type type;
    /**
     * \brief Type of the Option Template
     * \note Valid only when #type == ::IPX_TYPE_TEMPLATE_OPTS
     * \see #ipx_template_opts_type
     */
    uint32_t opts_types;

    /** Template ID                                                                              */
    uint16_t id;
    /**
     * \brief Features specific for this template.
     * The flags argument contains a bitwise OR of zero or more of the flags defined in
     * #ipx_template_features enumeration.
     */
    ipx_template_flag_t flags;

    /**
     * \brief Length of a data record using this template.
     * \warning If the template has at least one Information Element of variable-length encoding,
     *   i.e. #flags \& ::IPX_TEMPLATE_HAS_DYNAMIC is true, this value represents
     *   the smallest possible length of corresponding data record. Otherwise represents real
     *   length of the data record.
     */
    uint16_t data_length;

    /** Raw binary copy of the template (starts with a header)                                   */
    struct raw_s {
        /** Pointer to the copy of template record (starts with a header)                        */
        uint8_t *data;
        /** Length of the record (in bytes)                                                      */
        uint16_t length;
    } raw; /**< Raw template record                                                              */

    /**
     * \brief Time information related to Exporting Process
     * \warning All timestamps (seconds since UNIX epoch) are based on "Export Time" from the
     *   IPFIX message header.
     */
    struct time_s {
        /** The first reception                                                                  */
        uint32_t first_seen;
        /** The last reception (a.k.a. refresh time)                                             */
        uint32_t last_seen;
        /** End of life (the time after which the template is not valid anymore) (UDP only)      */
        uint32_t end_of_life;
    } time;

    /**
     * Total number of fields
     * If the value is zero, this template is so-called Template Withdrawal.
     */
    uint16_t fields_cnt_total;
    /** Number of scope fields (first N records of the Options Template)                         */
    uint16_t fields_cnt_scope;

    /**
     * Array of parsed fields.
     * This element MUST be the last element in this structure.
     */
    struct ipx_tfield fields[1];
};

/**
 * \brief Parse an IPFIX template
 *
 * Try to parse the template from a memory pointed by \p ptr of at most \p len bytes. Typically
 * during processing of a (Options) Template Set, the parameter \p len is length to the end of
 * the (Options) Template Set. After successful parsing, the function will set the value to
 * the real length of the raw template (in octets). The \p len can be, therefore, used to jump to
 * the beginning of the next template definition.
 *
 * Some information of the template structure after parsing the template are still unknown.
 * These fields are set to default values:
 *   - All timestamps (ipx_template#time). Default values are zeros.
 *   - References to IE definition (ipx_tfield#def). Default values are NULL.
 *   - Some template field flags (i.e. ipx_tfield#flags): (Default state is not set)
 *     ::IPX_TFIELD_STRUCTURED, ::IPX_TFIELD_REVERSE and ::IPX_TFIELD_FLOW_KEY
 *   - Some global template flags (i.e. ipx_template#flags): (Default state is not set)
 *     ::IPX_TEMPLATE_HAS_REVERSE, ::IPX_TEMPLATE_HAS_STRUCT and ::IPX_TEMPLATE_HAS_FKEY
 *
 * These structure's members are usually filled and managed by a template manager (::ipx_tmgr_t)
 * to which the template is inserted.
 *
 * \param[in]     type  Type of template (::IPX_TYPE_TEMPLATE or ::IPX_TYPE_TEMPLATE_OPTIONS)
 * \param[in]     ptr   Pointer to the header of the template
 * \param[in,out] len   [in] Maximal length of the raw template /
 *                      [out] real length of the raw template in  octets (see notes)
 * \param[out]    tmplt Parsed template (automatically allocated)
 * \return On success, the function will set parameters \p tmplt, \p len and return #IPX_OK.
 *   Otherwise, the parameters will be unchanged and the function will return #IPX_ERR_FORMAT or
 *   #IPX_ERR_NOMEM.
 */
IPX_API int
ipx_template_parse(enum ipx_template_type type, const void *ptr, uint16_t *len,
    struct ipx_template **tmplt);

/**
 * \brief Create a copy of a template structure
 *
 * \warning Keep in mind that references to the definitions of template fields will be preserved.
 *   If you do not have control over a corresponding Information Element manager, you should
 *   remove the references using the ipx_template_define_ies() function.
 * \param[in] tmplt Original template
 * \return Pointer to the copy or NULL (in case of memory allocation error).
 */
IPX_API struct ipx_template *
ipx_template_copy(const struct ipx_template *tmplt);

/**
 * \brief Destroy a template
 * \param[in] tmplt Template instance
 */
IPX_API void
ipx_template_destroy(struct ipx_template *tmplt);

/**
 * \brief Find the first occurence of an Information Element in a template
 * \param[in] tmplt Template structure
 * \param[in] en    Enterprise Number
 * \param[in] id    Information Element ID
 * \return Pointer to the IE or NULL.
 */
IPX_API struct ipx_tfield *
ipx_template_find(struct ipx_template *tmplt, uint32_t en, uint16_t id);

/**
 * \copydoc ipx_template_find()
 */
IPX_API const struct ipx_tfield *
ipx_template_cfind(const struct ipx_template *tmplt, uint32_t en, uint16_t id);

/**
 * \brief Add references to Information Element definitions and update corresponding flags
 *
 * The function will try to find a definition of each template field in a manager of IE definitions
 * based on Information Element ID and Private Enterprise Number of the template field.
 * Template flags (i.e. ::IPX_TEMPLATE_HAS_REVERSE and ::IPX_TEMPLATE_HAS_STRUCT) and
 * field flags (i.e. ::IPX_TFIELD_STRUCTURED and ::IPX_TFIELD_REVERSE) that can be determined
 * from the definitions will be set appropriately.
 *
 * \note If the manager is _not defined_ (i.e. NULL) and preserve mode is _disabled_, all the
 *   template field references to the definitions will be removed and corresponding flags will be
 *   cleared.
 * \note If the manager is _defined_ and preserve mode is _disabled_, all the template field
 *   references to the definitions will be updated. If any field doesn't have a corresponding
 *   definition in the user defined manager, the old reference will be removed.
 * \note If the manager is _defined_ and preserve mode is _enabled_, the function will update only
 *   template fields without the known references. This allows you to use multiple definition
 *   managers (for example, primary and secondary) at the same time.
 * \note If the manager is _not_defined_ and preserve mode is _enabled_, the function does nothing.
 *
 * \param[in] tmplt    Template structure
 * \param[in] iemgr    Manager of Information Elements definitions (can be NULL)
 * \param[in] preserve If any field already has a reference to a definition, do not replace it with
 *   a new definition. In other words, add definitions only to undefined fields.
 */
IPX_API void
ipx_template_define_ies(struct ipx_template *tmplt, const fds_iemgr_t *iemgr, bool preserve);

/**
 * \brief Add a flow key
 *
 * Flowkey is a set of bit fields that is used for marking the Information Elements of a Data
 * Record that serve as Flow Key. Each bit represents an Information Element in the Data Record,
 * with the n-th least significant bit representing the n-th Information Element. A bit set to
 * value 1 indicates that the corresponding Information Element is a Flow Key of the reported
 * Flow. A bit set to value 0 indicates that this is not the case. For more information, see
 * RFC 7011, Section 4.4
 *
 * The function will set flow key flag (i.e.::IPX_TFIELD_FLOW_KEY) to corresponding template
 * fields and the global template flag ::IPX_TEMPLATE_HAS_FKEY.
 * \note If the \p flowkey parameter is zero, the flags will be clear from the template and the
 *   fields.
 *
 * \param[in] tmplt   Template structure
 * \param[in] flowkey Flow key
 * \return On success, the function will return #IPX_OK. Otherwise, if the \p flowkey tries to
 *   set non-existent template fields as flow keys, the function will return #IPX_ERR_FORMAT and
 *   no modification will be performed.
 */
IPX_API int
ipx_template_define_flowkey(struct ipx_template *tmplt, uint64_t flowkey);

/**
 * \brief Compare templates (only based on template fields)
 *
 * Only raw templates are compared i.e. everything is ignored except Template ID
 * and template fields (Information Element ID, Private Enterprise Number and length)
 * \param t1 First template
 * \param t2 Second template
 * \return The function returns an integer less than, equal to, or greater than zero if the first
 *   template is found, respectively, to be less than, to match, or be greater than the second
 *   template.
 */
IPX_API int
ipx_template_cmp(const struct ipx_template *t1, const struct ipx_template *t2);

#ifdef __cplusplus
}
#endif
#endif // IPFIXCOL_TEMPLATE_H
