/**
 * @file src/core/netflow2ipfix/netflow2ipfix.h
 * @author Lukas Hutak <lukas.hutak@cesnet.cz>
 * @brief Main NetFlow v5/v9 to IPFIX converter functions (header file)
 * @date 2018-2019
 *
 * Copyright(c) 2019 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IPFIXCOL2_NETFLOW2IPFIX_H
#define IPFIXCOL2_NETFLOW2IPFIX_H

#include <stdint.h>
#include <stddef.h>
#include <ipfixcol2.h>

/**
 * @defgroup nf5_to_ipfix NetFlow v5 to IPFIX
 * @brief Conversion from NetFlow v5 Messages to IPFIX Messages
 *
 * The converter helps to convert a stream of NetFlow Messages from a NetFlow exporter to stream
 * of IPFIX Messages. Messages are processed individually and should be passed to the
 * converter in the order send by the exporter. If it is necessary to convert streams from
 * multiple exporters at time, you MUST create an independent instance for each stream.
 *
 * @note
 *   If stream of NetFlow packets contains a soft error (such as missing one or more packets,
 *   reordered packets), a warning message is printed on the standard output. Generated stream of
 *   IPFIX Messages don't contain these error (i.e. independent sequence numbers are generated)
 *
 * @{
 */

/// Auxiliary definition of NetFlow v5 to IPFIX converter internals
typedef struct ipx_nf5_conv ipx_nf5_conv_t;

/**
 * @brief Initialize NetFlow v5 to IPFIX converter
 *
 * @note
 *   Template refresh interval (@p tmplt_refresh) refers to exporter timestamps in NetFlow
 *   Messages to convert, not to wall-clock time.
 * @param[in] ident         Instance identification (only for log messages!)
 * @param[in] vlevel        Verbosity level of the converter (i.e. amount of log messages)
 * @param[in] tmplt_refresh Template refresh interval (seconds, 0 == disabled)
 * @param[in] odid          Observation Domain ID of IPFIX Messages (e.g. 0)
 * @return Pointer to the converter or NULL (memory allocation error)
 */
ipx_nf5_conv_t *
ipx_nf5_conv_init(const char *ident, enum ipx_verb_level vlevel, uint32_t tmplt_refresh,
    uint32_t odid);

/**
 * @brief Destroy NetFlow v5 to IPFIX converter
 * @param[in] conv Converter to destroy
 */
void
ipx_nf5_conv_destroy(ipx_nf5_conv_t *conv);

/**
 * @brief Convert NetFlow v5 message to IPFIX message
 *
 * The function accepts a message wrapper @p wrapper that should hold a NetFlow v5 Message.
 * If the NetFlow Message is successfully converted, a content of the wrapper is replaced
 * with the IPFIX Message and the original NetFlow Message is not accessible anymore and it is
 * freed.
 *
 * @note
 *   In case of an error (i.e. return code different from #IPX_OK) the original NetFlow Message
 *   in the wrapper is untouched.
 * @note
 *   Sequence number of the first IPFIX Message is deduced from the first NetFlow message to
 *   convert. Sequence numbers of following converted messages is incremented independently
 *   on the original NetFlow sequence number to avoid creation of invalid messages. In other words,
 *   missing or reordered NetFlow messages don't affect correctness of the IPFIX stream.
 *
 * @param[in] conv    Message converter
 * @param[in] wrapper Message wrapper
 * @return #IPX_OK on success
 * @return #IPX_ERR_FORMAT in case of invalid NetFlow Message format
 * @return #IPX_ERR_NOMEM in case of a memory allocation error
 */
int
ipx_nf5_conv_process(ipx_nf5_conv_t *conv, ipx_msg_ipfix_t *wrapper);

/**
 * @brief Change verbosity level
 *
 * @param[in] conv   Message converter
 * @param[in] v_new  New verbosity level
 */
void
ipx_nf5_conv_verb(ipx_nf5_conv_t *conv, enum ipx_verb_level v_new);

/**
 * @}
 */

/**
 * @defgroup nf9_to_ipfix NetFlow v9 to IPFIX
 * @brief Conversion from NetFlow v9 Messages to IPFIX Messages
 *
 * The converter helps to convert a stream of NetFlow Messages from combination of a NetFlow
 * exporter and a Source ID (a.k.a. Observation Domain ID) to stream of IPFIX Messages. Messages
 * are processed individually and should be passed to the converter in the order send by the
 * exporter. If it is necessary to convert streams from the same exporter with different Source
 * IDs or from multiple exporters at time, you MUST create an independent instance for each
 * stream (i.e. combination of an Exporter and Source ID).
 *
 * @note
 *   NetFlow Field Specifiers with ID > 127 are not backwards compatible with IPFIX Information
 *   Elements, therefore, after conversion these specifiers are defined as Enterprise Specific
 *   fields with Enterprise Number 4294967294 (128 <= ID <= 32767) or Enterprise Number 4294967295
 *   (32768 <= ID <= 65535). In the latter case, the ID of the field is changed to fit into
 *   range 0..32767: newID = oldID - 32768.
 *
 * @note
 *   In the context of IPFIX protocol, the Source ID is referred as Observation Domain ID (ODID)
 *
 * @{
 */

/// Auxiliary definition of NetFlow v9 to IPFIX converter internals
typedef struct ipx_nf9_conv ipx_nf9_conv_t;

/**
 * @brief Initialize NetFlow v9 to IPFIX converter
 *
 * @param[in] ident  Instance identification (only for log messages!)
 * @param[in] vlevel Verbosity level of the converter (i.e. amount of log messages)
 * @return Pointer to the converter or NULL (memory allocation error)
 */
ipx_nf9_conv_t *
ipx_nf9_conv_init(const char *ident, enum ipx_verb_level vlevel);

/**
 * @brief Destroy NetFlow v9 to IPFIX converter
 * @param[in] conv Converter to destroy
 */
void
ipx_nf9_conv_destroy(ipx_nf9_conv_t *conv);

/**
 * @brief Convert NetFlow v9 Message to IPFIX Message
 *
 * The function accepts a message wrapper @p wrapper that should hold a NetFlow v9 Message.
 * If the NetFlow Message is successfully converted, a content of the wrapper is replaced
 * with the IPFIX Message and the original NetFlow Message is not accessible anymore and it is
 * freed.
 *
 * @note
 *   In case of an error (i.e. return code different from #IPX_OK) the original NetFlow Message
 *   in the wrapper is untouched.
 * @note
 *   After conversion the IPFIX Message is not ready to be used for accessing flow records,
 *   it MUST be processed by the IPFIX Parser first.
 * @note
 *   Sequence numbers of IPFIX Messages are incremented independently on NetFlow messages to
 *   convert and starts from 0. In other words, missing or reordered NetFlow messages don't
 *   affect correctness of the IPFIX stream.
 *
 * @param[in] conv    Message converter
 * @param[in] wrapper Message wrapper
 * @return #IPX_OK on success
 * @return #IPX_ERR_FORMAT in case of invalid NetFlow Message format
 * @return #IPX_ERR_NOMEM in case of a memory allocation error
 */
int
ipx_nf9_conv_process(ipx_nf9_conv_t *conv, ipx_msg_ipfix_t *wrapper);

/**
 * @brief Change verbosity level
 *
 * @param[in] conv   Message converter
 * @param[in] v_new  New verbosity level
 */
void
ipx_nf9_conv_verb(ipx_nf9_conv_t *conv, enum ipx_verb_level v_new);

/**
 * @}
 */

#endif // IPFIXCOL2_NETFLOW2IPFIX_H
