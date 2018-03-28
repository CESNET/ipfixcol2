/**
 * \file include/ipfixcol2/message_garbage.h
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Garbage message (header file)
 * \date 2016
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

#ifndef IPX_MESSAGE_GARBAGE_H
#define IPX_MESSAGE_GARBAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup ipxGarbageMessage Garbage message
 * \ingroup ipxGeneralMessage
 *
 * \brief Garbage message interface
 *
 * A message for destruction of objects that are intended to be destroyed, but can be referenced
 * by other still existing objects farther down in the collector's pipeline.
 *
 * For example, a withdrawn template that can be referenced by still existing packets
 * _farther down_ in the pipeline. This message will call a callback to destroy the object before
 * the destruction of this message. It ensures that there are no objects _farther down_ in the
 * pipeline with references to this object, because that objects have been already destroyed.
 *
 * \warning Already destroyed objects MUST never be used again.
 * \remark Identification type of this message is #IPX_MSG_GARBAGE.
 *
 * @{
 */

/** \brief The type of garbage message                                      */
typedef struct ipx_msg_garbage ipx_msg_garbage_t;

#include <ipfixcol2/message.h>

/**
 * \typedef ipx_msg_garbage_cb
 * \brief Object destruction callback for a garbage message type
 * \param[in,out] object Object to be destroyed
 */
typedef void (*ipx_msg_garbage_cb)(void *object);

/**
 * \brief Create a garbage message
 *
 * At least callback must be always defined.
 * \param[in,out] object   Pointer to the object to be destroyed (can be NULL)
 * \param[in]     callback Object destruction function
 * \return On success returns a pointer to the message. Otherwise returns NULL.
 */
IPX_API ipx_msg_garbage_t *
ipx_msg_garbage_create(void *object, ipx_msg_garbage_cb callback);

/**
 * \brief Destroy a garbage message
 *
 * First, call the destructor of an internal object and than destroy itself.
 * \param[in,out] msg Pointer to the message
 */
IPX_API void
ipx_msg_garbage_destroy(ipx_msg_garbage_t *msg);

/**
 * \brief Cast from a garbage message to a base message
 * \param[in] msg Pointer to the garbage message
 * \return Pointer to the base message.
 */
static inline ipx_msg_t *
ipx_msg_garbage2base(ipx_msg_garbage_t *msg)
{
    return (ipx_msg_t *) msg;
}

// ------------------------------------------------------------------------------------------------

/** Garbage container */
typedef struct ipx_gc ipx_gc_t;

/**
 * \brief Create a garbage container
 * \return Pointer to the container or NULL (memory allocation failure)
 */
IPX_API ipx_gc_t *
ipx_gc_create();

/**
 * \brief Destroy a garbage container
 *
 * Before the container is destroyed, all internal garbage is destroyed by calling appropriate
 * callback function.
 * \note If \p gc is NULL, nothing is done.
 * \param[in] gc Container
 */
IPX_API void
ipx_gc_destroy(ipx_gc_t *gc);

/**
 * \brief Release garbage
 *
 * The container is marked as empty.
 * \warning Garbage is NOT destroyed! No callback function is called!
 * \param[in] gc Container
 */
IPX_API void
ipx_gc_release(ipx_gc_t *gc);

/**
 * \brief Add garbage to a container
 *
 * There are no changes in the container in case of error.
 * \note If \p data is NULL, nothing is added and success is returned.
 * \param[in] gc   Container
 * \param[in] data Data to destroy (can be NULL)
 * \param[in] cb   Callback function that will destroy \p data (can NOT be NULL)
 * \return #IPX_OK on success
 * \return #IPX_ERR_NOMEM if a memory allocation has occurred
 * \return #IPX_ERR_ARG if \p cb function is undefined
 */
IPX_API int
ipx_gc_add(ipx_gc_t *gc, void *data, ipx_msg_garbage_cb cb);

/**
 * \brief Request that a container capacity be at least enough to contain N elements
 *
 * There are no changes in the container in case of error.
 * \param[in] gc Container
 * \param[in] n  Minimum capacity of the container
 * \return #IPX_OK on success
 * \return #IPX_ERR_NOMEM if a memory allocation has occurred
 */
IPX_API int
ipx_gc_reserve(ipx_gc_t *gc, size_t n);

/**
 * \brief Is the container empty?
 * \param[in] gc Container
 * \return True or false
 */
IPX_API bool
ipx_gc_empty(const ipx_gc_t *gc);

/**
 * \brief Convert a container to a garbage message
 * \warning
 *   Do NOT destroy the container after the message is successfully generated. However, there are
 *   no changes in the container in case of error and you have to free it manually, if possible.
 * \param[in] gc Container
 * \return Pointer to a newly created garbage message on success and the container doesn't exists
 *   anymore.
 * \return NULL in case of memory allocation error
 */
IPX_API ipx_msg_garbage_t *
ipx_gc_to_msg(ipx_gc_t *gc);

/**@}*/

#ifdef __cplusplus
}
#endif
#endif // IPX_MESSAGE_GARBAGE_H
