/**
 * \file modifier.h
 * \author Adam Zvara <xzvara01@stud.fit.vutbr.cz>
 * \brief Modifier component (header file)
 * \date 2023
 */

#ifndef IPX_MODIFIER_H
#define IPX_MODIFIER_H

#include <ipfixcol2/api.h>

/** Internal declaration of modifier */
typedef struct ipx_modifier ipx_modifier_t;

/**
 * \brief IPFIX field specifier declaration for modifier
 * \see #fds_ipfix_tmplt_ie_u
 */
struct ipx_modifier_field {
    /** Information element ID      */
    uint16_t id;
    /** Length of field             */
    uint16_t length;
    /** Enterprise Number           */
    uint32_t en;
};

/**
 * \brief Output values from callback function
 * \warning Maximum size of stored data can be 65535 bytes (UINT16_MAX)
 */
struct ipx_modifier_output {
    /** Raw output data  */
    uint8_t raw[UINT16_MAX];

    /** Length of returned data
     *  If value is not available, length contains negative
     *  number and represents error code */
    int length;
};

/**
 * \brief Callback signature for adding new elements data record
 * \note output size is equal to number of new fields
 * \note Any non-negative value in output[i].length means that
 * data in output at given position is valid and will be appended to IPFIX message
 * \return #IPX_OK if function was successfull, otherwise any non-zero value
 */
typedef int (*modifier_adder_cb_t)(const struct fds_drec *rec,
    struct ipx_modifier_output output[], void *cb_data);

/**
 * \brief Callback signature for filtering elements from data record
 * \note filter size is equal to field count in record's template
 * \note Any non-zero value in filter indicates that given field
 *  is be filtered from original record (and template)
*/
typedef void (*modifier_filter_cb_t)(const struct fds_drec *rec,
    uint8_t *filter, void *cb_data);

/**
 * \brief Initialize modifier
 *
 * \note If \p vlevel is NULL, default collector verbosity is used.
 *
 * \param[in] fields      Array of new fields which will be added to original message
 * \param[in] fields_size Number of items in fields array
 * \param[in] cb_data     Data which can be used in callback function (cb_adder or cb_filter)
 * \param[in] iemgr       IE element manager
 * \param[in] vlevel      Verbosity level (can be NULL)
 * \param[in] ident       Identification string (of parent context)
 * \return Pointer to created modifier or NULL (memory allocation error)
 */
IPX_API ipx_modifier_t *
ipx_modifier_create(const struct ipx_modifier_field *fields, size_t fields_size,
    void *cb_data, const fds_iemgr_t *iemgr, const enum ipx_verb_level *vlevel, const char *ident);

/**
 * \brief Destroy modifier and free its memory
 *
 * \param[in] modifier Pointer to initialized modifier
 * \return Garbage message or NULL, if no garbage was found
 */
IPX_API void
ipx_modifier_destroy(ipx_modifier_t *mod);

/**
 * \brief Change verbosity level
 *
 * If \p v_new is non-NULL, the new level is set from \p v_new.
 * If \p v_old is non-NULL, the previous level is saved in \p v_old.
 *
 * \param[in] mod    Modifier component
 * \param[in] v_new  New verbosity level (can be NULL)
 * \param[in] v_old  Old verbosity level (can be NULL)
 */
IPX_API void
ipx_modifier_verb(ipx_modifier_t *mod, enum ipx_verb_level *v_new, enum ipx_verb_level *v_old);

/**
 * \brief Set modifier's adder callback
 *
 * Adder callback is function used for setting values of fields, which will
 * be added to original record passed to modifier.
 *
 * \param[in] modifier  Initialized modifier
 * \param[in] adder     Callback for adding new elements
 */
IPX_API void
ipx_modifier_set_adder_cb(ipx_modifier_t *mod, const modifier_adder_cb_t adder);

/**
 * \brief Set modifier's filter callback
 *
 * Filter callback is function used for setting filter array indicating
 * which fields and records will be deleted from record.
 *
 * \param[in] modifier  Initialized modifier
 * \param[in] filter    Callback for adding new elements
 */
IPX_API void
ipx_modifier_set_filter_cb(ipx_modifier_t *mod, const modifier_filter_cb_t filter);

/**
 * \brief Return modifier's active template manager
 *
 * \param[in] mod Modifier
 * \return Pointer to template manager (or NULL if not modifier is NULL or manager is not set)
 */
IPX_API const fds_tmgr_t *
ipx_modifier_get_manager(ipx_modifier_t *mod);

/**
 * \brief Return modifier's information element manager
 *
 * \param[in] mod Modifier
 * \return Pointer to element manager (or NULL if modifier is NULL)
 */
IPX_API const fds_iemgr_t *
ipx_modifier_get_iemgr(ipx_modifier_t *mod);

/**
 * \brief Set modifier's information element manager
 *
 * \param[in] mod   Modifier
 * \param[in] iemgr Element manager (can be NULL)
 * \return #IPX_OK on success, otherwise #IPX_ERR_ARG when modifier is NULL
 */
IPX_API int
ipx_modifier_set_iemgr(ipx_modifier_t *mod, struct fds_iemgr *iemgr);

/**
 * \brief Filter data from parsed IPFIX data record and modify records template
 *
 * Modified record data is replaced in place and new template is created. Original
 * template in record is replaced with new template.
 *
 * \warning This function removes snapshot from data record, which needs to be set again!
 *
 * \param[in,out]   rec     Original/parsed data record
 * \param[in]       filter  Filter array
 * \return #IPX_OK on success
 * \return #IPX_ERR_ARG if any argument is NULL
 * \return #IPX_ERR_NOMEM on memory allocation error
 */
IPX_API int
ipx_modifier_filter(struct fds_drec *rec, uint8_t *filter);

/**
 * \brief Append data to parsed IPFIX data record and modify records template
 *
 * Modified record data is replaced in place and new template is created. Original
 * template in record is replaced with new template.
 *
 * \warning This function removes snapshot from data record, which needs to be set again!
 *
 * \param[in,out]   rec         Original/parsed data record
 * \param[in]       fields      Array of new template fields
 * \param[in]       buffers     Output buffers
 * \param[in]       fields_cnt  Number of items in fields (and buffers)
 * \return #IPX_OK on success
 * \return #IPX_ERR_ARG if any argument is NULL
 * \return #IPX_ERR_NOMEM on memory allocation error
 */
IPX_API int
ipx_modifier_append(struct fds_drec *rec, const struct ipx_modifier_field *fields,
    const struct ipx_modifier_output *buffers, const size_t fields_cnt);

/**
 * \brief Add new session context to modifier
 *
 * Context of current modified message contains information about transport session
 * and ODID, which identifies exporter. For each combination of transport session and ODID
 * a unique template manager is used. This function updates list of template
 * managers in modifier, creating new ones for previously unseen transport sessions
 * and sets export time extracted from message.
 *
 * \warning This function MUST be used before attempting to modify IPFIX message
 *
 * \param[in]  mod       Modifier
 * \param[in]  ipfix_msg IPFIX message before modification
 * \param[out] garbage   Potential garbage created by this function (NULL if none)
 * \return #IPX_OK if successful
 * \return #IPX_ERR_ARG when invalid arguments are passed
 * \return #IPX_ERR_FORMAT when trying to set export time in history for TCP session
 * \return #IPX_ERR_NOMEM on memory allocation error
 * \return #IPX_ERR_DENIED on unexpected error
 */
IPX_API int
ipx_modifier_add_session(struct ipx_modifier *mod, ipx_msg_ipfix_t *ipfix_msg, ipx_msg_garbage_t **garbage);

/**
 * \brief Remove transport session context from modifier
 *
 * \note Transport session is identified by session structure only, NOT ODID
 * which means, that removing session also removes every session + ODID pair
 * from modifier
 *
 * \warning If modifier's current session context (modifier->curr_ctx) is from
 * removed session, the pointer becomes NULL!
 *
 * \param[in]  mod       Modifier
 * \param[in]  session   Transport session to be removed
 * \param[out] garbage   Garbage message containing removed session or NULL
 * \return #IPX_OK on success, #IPX_ERR_NOTFOUND if session was not found in modifier
 */
IPX_API int
ipx_modifier_remove_session(struct ipx_modifier *mod, const struct ipx_session *session,
    ipx_msg_garbage_t **garbage);

/**
 * \brief Modify given records based on modifier configuration
 *
 * \warning Snapshot in modified record becomes invalid!
 * \warning Function ipx_modifier_set_time() must be called before attempting
 *  to modify record for first time!
 * \note Adding callback or filter callback need to be added before calling
 *  this function with ipx_modifier_set_adder_cb() or ipx_modifier_set_filter_cb()
 * \note Modified templates are stored in modifier template managers
 *
 * \param[in]  modifier Modifier component
 * \param[in]  record   Parsed IPFIX data record
 * \param[out] garbage  Potential garbage created by this function (NULL if none)
 *
 * \return Modified data record with added/filtered fields based on modifier configuration
 * \return NULL if any error occured
 */
IPX_API struct fds_drec *
ipx_modifier_modify(ipx_modifier_t *mod, const struct fds_drec *record, ipx_msg_garbage_t **garbage);

#endif