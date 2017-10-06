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

/** \brief Template Field features                                                               */
enum ipx_tfield_features {
    /**
     * \brief Multiple occurrences of this Information Element (IE)
     *
     * If this flag is set, there are multiple occurrences of this IE anywhere in the template
     * to which the field belongs.
     */
    IPX_TFIELD_MULTI_IE = (1 << 0),
    /**
     * \brief The last occurrence of this Information Elements (IE)
     *
     * If this flags (is set, there are NO more occurrences of the IE with the same combination of
     * an Information Element ID and an Enterprise Number in a template to which the field belongs.
     * In other words, if this flag is NOT set, there is at least one IE with the same definition
     * and with _higher_ index in the template.
     * \note This flag is also set if there are NOT multiple occurrences of the same IE.
     */
    IPX_TFIELD_LAST_IE = (1 << 1),
    /**
     * \brief Reverse Information Element
     *
     * An Information Element defined as corresponding to a normal (or forward) Information
     * Element, but associated with the reverse direction of a Biflow.
     * \note To distinguish whether the IE is reverse or not, an external database of IEs must be
     *   used. For example, use the IE manager distributed with libfds. In other words, this
     *   information is not a part of template definition.
     */
    IPX_TFIELD_REVERSE = (1 << 2),
    /**
     * \brief TODO
     */
    IPX_TFIELD_FLOW_KEY = (1 << 3)
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
    uint16_t flags;
    /**
     * Detailed definition of the element (data/semantic/unit type).
     * If the definition is missing in the configuration, the values is NULL.
     */
    const struct fds_iemgr_elem *def;
};

/** Types of IPFIX (Options) Templates                                                           */
enum ipx_template_type {
    IPX_TYPE_TEMPLATE,         /**< Definition of Template                                       */
    IPX_TYPE_TEMPLATE_OPTIONS, /**< Definition of Options Template                               */
    IPX_TYPE_TEMPLATE_UNDEF    /**< For internal usage                                           */
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
    IPX_TEMPLATE_IS_BIFLOW = (1 << 2), // TODO: HAS_REVERSE?
    /** Template has know the flow key (at least one field is marked as a Flow Key)              */
    IPX_TEMPLATE_HAS_FKEY = (1 << 3)
    // Add new ones here...
};

/**
 * \brief Structure for a parsed IPFIX template
 *
 * This dynamically sized structure wraps a parsed copy of an IPFIX template.
 */
struct ipx_template {
    /** Type of the template                                                                     */
    enum ipx_template_type type;
    /**
     * \brief Type of the Option Template
     * \note Valid only when #type == ::IPX_TYPE_TEMPLATE_OPTIONS
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
    uint16_t flags;

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

    /** Total number of fields                                                                   */
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
 * Try to parse the template from a memory pointed by \p ptr of at most \p size bytes. Typically
 * during processing of a (Options) Template Set, the parameter \p size is length to the end of
 * the (Options) Template Set. After successful parsing, the function will set the value to
 * the real size of the raw template (in octets). The \p size can be, therefore, used to jump to
 * the beginning of the next template definition.
 *
 * Some information of the template structure after parsing the template are still unknown.
 * These fields are set to default values:
 *   - All timestamps (ipx_template#time). Default values are zeros.
 *   - References to IE definition (ipx_tfield#def). Default values are NULL.
 *   - Whether an IE is reverse or not (ipx_tfield#flags, flag ::IPX_TFIELD_REVERSE) as well as
 *     whether the template is Biflow or not (ipx_template#flags, flag ::IPX_TEMPLATE_IS_BIFLOW).
 *     These flags are unset, by default.
 * These structure's members are filled and managed by a template manager (::ipx_tmgr_t) to which
 * the template is inserted.
 *
 * \param[in]     type  Type of template (::IPX_TYPE_TEMPLATE or ::IPX_TYPE_TEMPLATE_OPTIONS)
 * \param[in]     ptr   Pointer to the header of the template
 * \param[in,out] size  Maximal size of the raw template / real size of the raw template in octets
 *                      (see notes)
 * \param[out]    tmplt Parsed template (automatically allocated)
 * \return On success, the function will set parameters \p tmplt, \p size and return #IPX_OK.
 *   Otherwise the parameters are unchanged and the function will return #IPX_ERR_FORMAT or
 *   #IPX_ERR_NOMEM.
 */
IPX_API int
ipx_template_parse(enum ipx_template_type type, const void *ptr, uint16_t *size,
    struct ipx_template **tmplt);

/**
 * \brief Destroy a template
 * \param[in] tmplt Template instance
 */
IPX_API void
ipx_template_destroy(struct ipx_template *tmplt);

/**
 * TODO: dokoncit definici... pokud polozka neni null, bude preskocena
 * \brief
 * \param[in] tmplt
 * \param[in] iemgr
 */
IPX_API void
ipx_template_define_ie(struct ipx_template *tmplt, const fds_iemgr_t *iemgr);

// TODO: bude tu i něco jako flag pro zachování ukazatelů na definice nebo ne?
IPX_API struct ipx_template *
ipx_template_copy(const struct ipx_template *tmplt);

// TODO: pridat definici... bude to porovnání pouze na úrovni raw zaznamů? nebo něco složitějšího?
IPX_API int
ipx_template_cmp(const struct ipx_template *t1, const struct ipx_template *t2);

// TODO: pridat definici
IPX_API int
ipx_template_cmp_raw(const struct ipx_template *t1, const void *ptr, uint16_t size);

/**
 * TODO: dokoncit definici
 * \note For more information, see RFC 7011, Section 4.4
 * \param[in] tmplt
 * \param[in] flowkey
 * \return
 */
IPX_API int
ipx_template_set_flowkey(struct ipx_template *tmplt, uint64_t flowkey);

#ifdef __cplusplus
}
#endif
#endif // IPFIXCOL_TEMPLATE_H
