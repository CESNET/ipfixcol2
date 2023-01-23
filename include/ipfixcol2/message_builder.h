/**
 * \file message_builder.h
 * \author Adam Zvara <xzvara01@stud.fit.vutbr.cz>
 * \brief Message builder (header file)
 * \date 2023
 */

#ifndef IPX_MESSAGE_BUILDER_H
#define IPX_MESSAGE_BUILDER_H

#include <ipfixcol2/api.h>

/** Internal declaration of message builder */
typedef struct ipx_msg_builder ipx_msg_builder_t;

/**
 * \brief Create new message builder
 *
 * \return Pointer to new message builder, otherwise NULL (memory allocation error)
 */
IPX_API ipx_msg_builder_t *
ipx_msg_builder_create();

/**
 * \brief Destroy message builder
 *
 * \warning This function does NOT free message pointer allocated in builder.
 *  Raw message is usually freed by calling ipx_msg_ipfix_destroy() on created
 *  wrapper or user can call ipx_msg_builder_free_raw() BEFORE destroying builder.
 *
 * \param[in] builder Message builder
 */
IPX_API void
ipx_msg_builder_destroy(ipx_msg_builder_t *builder);

/**
 * \brief Free memory allocated for raw message in builder
 *
 * \param[in] builder Message builder
 */
IPX_API void
ipx_msg_builder_free_raw(ipx_msg_builder_t *builder);

/**
 * \brief Get current maximum length of IPFIX message in builder
 *
 * \param[in] builder Message builder
 * \return Maximum allowed size of IPFIX message in builder or 0 (builder is NULL)
 */
IPX_API size_t
ipx_msg_builder_get_maxlength(const ipx_msg_builder_t *builder);

/**
 * \brief Set maximum length of new IPFIX message in builder
 *
 * \param[in] builder    Message builder
 * \param[in] new_length New maximum length of IPFIX message
 */
IPX_API void
ipx_msg_builder_set_maxlength(ipx_msg_builder_t *builder, const size_t new_length);

/**
 * \brief Start creating new IPFIX message
 *
 * This function allocates memory for new raw message and copies original message
 * header into it. It MUST be called before trying to add sets/records into
 * message builder.
 *
 * \note If hints are not given (e.g hints == 0), maxbytes is used as
 *  allocation size for raw message
 *
 * \param[in] builder  Pointer to message builder
 * \param[in] hdr      Original raw message header
 * \param[in] maxbytes Maximum length of new message (without IPFIX header)
 * \param[in] hints    Allocation size of new raw message
 * \return #IPX_OK on success
 * \return #IPX_ERR_ARG on invalid argument (NULL pointers or maxbytes < IPFIX header length)
 * \return #IPX_ERR_NOMEM on failed memory allocation
 */
IPX_API int
ipx_msg_builder_start(ipx_msg_builder_t *builder, const struct fds_ipfix_msg_hdr *hdr,
    const uint32_t maxbytes, const uint32_t hints);

/**
 * \brief Add new set to IPFIX message
 *
 * \warning When message length exceeds maxbytes, ipx_msg_builder_end needs to be called
 *  to create wrapper around message!
 *
 * \param[in] builder Pointer to builder
 * \param[in] id      ID of new set
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED when message length would exceed maxbytes
 * \return #IPX_ERR_NOMEM on memory allocation error
 */
IPX_API int
ipx_msg_builder_add_set(struct ipx_msg_builder *builder, const uint16_t id);

/**
 * \brief Add new data record to IPFIX message
 *
 * \warning When message length exceeds maxbytes, ipx_msg_builder_end needs to be called
 *  to create wrapper around message!
 * \note New set is added automatically, there is no need to call add_set before calling
 *  this function
 *
 * \param[in] builder Pointer to message builder
 * \param[in] record  Pointer to data record
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED when message length would exceed maxbytes
 * \return #IPX_ERR_NOMEM on memory allocation error
 */
IPX_API int
ipx_msg_builder_add_drec(ipx_msg_builder_t *builder, const struct fds_drec *record);

/**
 * \brief Create wrapper around raw IPFIX message in message builder
 *
 * \param[in] builder    Pointer to message builder
 * \param[in] plugin_ctx Plugin context
 * \param[in] msg_ctx    Message context (from original IPFIX message)
 * \return Pointer to allocated IPFIX wrapper or NULL
 */
IPX_API ipx_msg_ipfix_t *
ipx_msg_builder_end(const ipx_msg_builder_t *builder, const ipx_ctx_t *plugin_ctx,
    const struct ipx_msg_ctx *msg_ctx);

#endif