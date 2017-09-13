/**
 * \file   src/templater/tmpl_common.h
 * \author Michal Režňák
 * \brief  Common definitions for templater
 * \date   8/21/17
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

#ifndef TEMPLATER_COMMON_H
#define TEMPLATER_COMMON_H

#include <stdbool.h>
#include <libfds/iemgr.h>
#include <ipfixcol2/source.h>
#include <ipfixcol2/templater.h>
#include <ipfixcol2/common.h>
#include "tmpl_algorithms.h"

/** Size of the template head (ID + count) in bytes                       */
#define TEMPL_HEAD_SIZE      ((uint16_t) 4)
/** Size of the options template head (ID + count + scope_count) in bytes */
#define OPTS_TEMPL_HEAD_SIZE ((uint16_t) 6)
/** Size of the template set head (type + length) in bytes                */
#define TEMPL_SET_HEAD_SIZE  ((uint16_t) 4)
/** Size of the int32_t in bytes                                          */
#define INT32_SIZE           (4)
/** Size of the int16_t in bytes                                          */
#define INT16_SIZE          (sizeof(uint16_t))

/** \brief Timestamps relating to a template record                          */
struct ipx_template_time {
    /** Timestamp of the first received (seconds since UNIX epoch)           */
    uint64_t first;
    /** Timestamp of the last received (seconds since UNIX epoch)            */
    uint64_t last; // TODO this will never change
    /**
     * Timestamp of the template withdrawal (in seconds ).
     * If equals to "0", then the template is still valid.
     */
    uint64_t end;
};

/**
 * \brief Structure for a parsed IPFIX template
 *
 * This dynamically sized structure wraps a parsed copy of IPFIX template.
 */
struct ipx_tmpl_template {
    /** Type of the template                                                 */
    enum IPX_TEMPLATE_TYPE template_type;
    /** Type of the Option Template.                                         */
    enum IPX_OPTS_TEMPLATE_TYPE options_type;

    /** Template ID                                                          */
    uint16_t id;
    /**
     * Length of a data record using this template.
     * \warning If the template has at least one element with variable-length
     *   (property has_dynamic), this value represents the smallest possible
     *   length of corresponding data record. Otherwise represents real-length.
     */
    uint16_t data_length;

    /** Raw template record (starts with a header)                           */
    struct raw_s {
        /** Pointer to the copy of template record (starts with a header)    */
        void     *data;
        /** Length of the record (in bytes)                                  */
        uint16_t  length;
    } raw; /**< Raw template record                                          */

    /** Template properties */
    struct properties_s {
        /**
         * Has multiple definitions of the same element (i.e. combination of
         * Information Element ID and Enterprise Number)
         * */
        bool has_multiple_defs;
        /** Has at least one variable-length element                         */
        bool has_dynamic;
    } properties; /**< Properties of the template                            */

    /**
     * Time information related to exporting process.
     * All timestamp (seconds since UNIX epoch) are based on "Export time"
     * from the IPFIX message header.
     */
    struct ipx_template_time time;

    /**
     * Number in which packet template was sent
     */
    uint64_t number_packet;

    /**
     * Previous version of the template.
     * When template is overwritten, than old template is not removed
     * but only moved to the linked list sorted by time.
     */
    struct ipx_tmpl_template *next;

    /** Number of scope fields (first N records of the template)             */
    uint16_t fields_cnt_scope;
    /** Total number of fields                                               */
    uint16_t fields_cnt_total;

    /**
     * Array of parsed fields.
     * This element MUST be the last element in this structure.
     */
    struct ipx_tmpl_template_field fields[1];
};

/** Properties of the templater                                                    */
struct tmpl_prop {
    uint64_t time;              /**< Time                                          */
    uint64_t count;             /**< Number of packets transferred via one session */
};

/** Flags which define templater's behaviour                                             */
struct tmpl_flags {
    bool can_overwrite;    /**< True if templater can overwrite defined template         */
    bool can_truly_remove; /**< True if template will be removed on next garbage_get     */
    bool care_count;       /**< True if templater should compare packet number (UDP)     */
    bool care_time;        /**< True if template should remove template after some delay */
    bool modified;         /**< True if templater was modified after last snapshot       */
};

/** Templater                                                                      */
struct ipx_tmpl {
    fds_iemgr_t          *iemgr;     /**< Iemgr that defines individual elements   */
    enum IPX_SESSION_TYPE type;      /**< Protocol (UDP, SCTP, ...)                */
    struct tmpl_prop      life;      /**< Statistics how long should template live */
    struct tmpl_prop      current;   /**< Current statistics defined by user       */
    struct tmpl_flags     flag;      /**< Define different behavior                */
    vectm_t              *templates; /**< Vector of parsed templates               */
    const ipx_tmpl_t     *snapshot;  /**< Snapshots 'current time' sorted          */
};

#endif // TEMPLATER_COMMON_H
