/**
 * @file src/core/netflow2ipfix/netflow_parsers.h
 * @author Lukas Hutak <lukas.hutak@cesnet.cz>
 * @brief NetFlow v9 parsers (header file)
 * @date 2018-2019
 *
 * Copyright(c) 2019 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IPFIXCOL2_NETFLOW9_PARSERS_H
#define IPFIXCOL2_NETFLOW9_PARSERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <ipfixcol2.h>
#include "netflow_structs.h"

/// Iterator over NetFlow v9 Sets in an NetFlow v9 Message
struct ipx_nf9_sets_iter {
    /// Pointer to the current FlowSet header
    const struct ipx_nf9_set_hdr *set;

    struct {
        /// Pointer to the start of the next FlowSet
        const uint8_t *set_next;
        /// First byte after the end of the message
        const uint8_t *msg_end;
        /// Error buffer
        const char *err_msg;
    } _private; ///< Internal structure (dO NOT use directly!)
};

/**
 * @brief Initialize NetFlow v9 FlowSet iterator
 * @warning
 *   After initialization the iterator has initialized only internal structures but public part
 *   is still undefined i.e. doesn't point to the first FlowSet in the Message. To get the first
 *   field see ipx_nf9_sets_iter_next().
 * @warning
 *   The NetFlow Message header is not checked. Make sure that its length is at
 *   least >= ::IPX_NF9_MSG_HDR_LEN.
 * @param[in] it       Uninitialized iterator structure
 * @param[in] nf9_msg  NetFlow v9 Message to iterate through
 * @param[in] nf9_size NetFlow v9 Message size
 */
void
ipx_nf9_sets_iter_init(struct ipx_nf9_sets_iter *it, const struct ipx_nf9_msg_hdr *nf9_msg,
    uint16_t nf9_size);

/**
 * @brief Get the next FlowSet in the Message
 *
 * Move the iterator to the next FlowSet in the order, If this function was not previously called
 * after initialization by ipx_nf9_sets_iter_init(), then the iterator will point to the first
 * FlowSet in the Message.
 *
 * @code{.c}
 *   struct ipx_nf9_msg_hdr *nf9_msg = ...;
 *   struct ipx_nf9_sets_iter it;
 *   ipx_nf9_sets_iter_init(&it, nf9_msg);
 *
 *   int rc;
 *   while ((rc = ipx_nf9_sets_iter_next(&it)) == IPX_OK) {
 *      // Add your code here...
 *   }
 *
 *   if (rc != IPX_EOC) {
 *      fprintf(stderr, "Error: %s\n", ipx_nf9_sets_iter_err(&it));
 *   }
 * @endcode
 * @param[in] it Pointer to the iterator
 * @return #IPX_OK on success an the next Set is ready to use.
 * @return #IPX_EOC if no more Sets are available.
 * @return #IPX_ERR_FORMAT if the format of the message is invalid (an appropriate error message
 *   is set - see ipx_nf9_sets_iter_err()).
 */
int
ipx_nf9_sets_iter_next(struct ipx_nf9_sets_iter *it);

/**
 * @brief Get the last error message
 * @note The message is statically allocated string that can be passed to other function even
 *   when the iterator doesn't exist anymore.
 * @param[in] it Iterator
 * @return The error message.
 */
const char *
ipx_nf9_sets_iter_err(const struct ipx_nf9_sets_iter *it);

// ------------------------------------------------------------------------------------------------

/// Iterator over Data Records in an Netflow v9 Data FlowSet
struct ipx_nf9_dset_iter {
    /// Start of the data record
    const uint8_t *rec;

    struct {
        /// Iterator flags
        uint16_t flags;
        /// Size of the data record (in bytes)
        uint16_t rec_size;
        /// Pointer to the start of the next record
        const uint8_t *rec_next;
        /// First byte after end of the data set
        const uint8_t *set_end;
        /// Error buffer
        const char *err_msg;
    } _private; ///< Internal structure (dO NOT use directly!)
};

/**
 * @brief Initialize NetFlow v9 Data records iterator
 *
 * The function takes the size of a data record that corresponds to the particular (Options)
 * Template. It only up to user to manager Templates and track size of data records.
 *
 * @warning
 *   After initialization the iterator has initialized only internal structures but public part
 *   is still undefined i.e. doesn't point to the first Record in the Data Set. To get the first
 *   field see ipx_nf9_dset_iter_next().
 * @warning
 *   Make sure that the length of allocated memory of the Set is at least the same as the length
 *   from the Set header before using the iterator. Not required if you got the FlowSet from
 *   ipx_nf9_sets_iter_next().
 * @param[in] it       Uninitialized structure
 * @param[in] set      Data FlowSet header to iterate through
 * @param[in] rec_size Size of data record
 */
void
ipx_nf9_dset_iter_init(struct ipx_nf9_dset_iter *it, const struct ipx_nf9_set_hdr *set,
    uint16_t rec_size);

/**
 * @brief Get the next Data Record in the Data Set
 *
 * Move the iterator to the next Record in the order, If this function was not previously called
 * after initialization by ipx_nf9_dset_iter_init(), then the iterator will point to the first
 * Record in the Data Set.
 *
 * @code{.c}
 *   struct ipx_nf9_set_hdr *data_set = ...;
 *   uint16_t rec_size = ...
 *   struct ipx_nf9_dset_iter it;
 *   ipx_nf9_dset_iter_init(&it, data_set, rec_size);
 *
 *   int rc;
 *   while ((rc = ipx_nf9_dset_iter_next(&it)) == IPX_OK) {
 *      // Add your code here...
 *   }
 *
 *   if (rc != IPX_EOC) {
 *      fprintf(stderr, "Error: %s\n", ipx_nf9_dset_iter_err(&it));
 *   }
 * @endcode
 * @param[in] it Pointer to the iterator
 * @return #IPX_OK on success and the next Record is ready to use.
 * @return #IPX_EOC if no more Records are available (the end of the Set has been reached).
 * @return #IPX_ERR_FORMAT if the format of the Data FlowSet is invalid (an appropriate error
 *   message is set - see ipx_nf9_dset_iter_err()).
 */
int
ipx_nf9_dset_iter_next(struct ipx_nf9_dset_iter *it);

/**
 * @brief Get the last error message
 * @note The message is statically allocated string that can be passed to other function even
 *   when the iterator doesn't exist anymore.
 * @param[in] it Iterator
 * @return The error message
 */
const char *
ipx_nf9_dset_iter_err(const struct ipx_nf9_dset_iter *it);

// ------------------------------------------------------------------------------------------------

/// Iterator over template records in an NetFlow v9 (Options) Template Set
struct ipx_nf9_tset_iter {
    union {
        /// Start of the Template Record            (scope_cnt == 0)
        const struct ipx_nf9_trec      *trec;
        /// Start of the Options Template Record    (scope_cnt >= 0)
        const struct ipx_nf9_opts_trec *opts_trec;
    } ptr; ///< Pointer to the start of the template record
    /// Size of the template record (in bytes)
    uint16_t size;
    /// Total field count (scope + non-scope field)
    uint16_t field_cnt;
    /**
     * Scope field count
     * @warning Unlike IPFIX, the value can be zero even if it is Options Template.
     */
    uint16_t scope_cnt;

    struct {
        /// Type of templates
        uint16_t type;
        /// Iterator flags
        uint16_t flags;
        /// Pointer to the start of the next record
        const uint8_t *rec_next;
        /// First byte after end of the data set
        const uint8_t *set_end;
        /// Error buffer
        const char *err_msg;
    } _private; ///< Internal structure (do NOT use directly!)
};

/**
 * @brief Initialize NetFlow v9 (Options) Template records iterator
 *
 * @note
 *   FlowSet ID of the @p set MUST be 0 (::IPX_NF9_SET_TMPLT) or 1 (::IPX_NF9_SET_OPTS_TMPLT).
 *   Otherwise behavior of the parser is undefined.
 * @warning
 *   After initialization the iterator has initialized only internal structures but public part
 *   is still undefined i.e. doesn't point to the first Record in the FlowSet. To get the
 *   first field see ipx_nf9_tset_iter_next().
 * @warning
 *   Make sure that the length of allocated memory of the Set is at least the same as the length
 *   from the Set header before using the iterator. Not required if you got the FlowSet from
 *   ipx_nf9_sets_iter_next().
 * @param[in] it  Uninitialized structure
 * @param[in] set (Options) Template FlowSet header to iterate through
 */
void
ipx_nf9_tset_iter_init(struct ipx_nf9_tset_iter *it, const struct ipx_nf9_set_hdr *set);

/**
 * @brief Get the next (Options) Template Record in the Data FlowSet
 *
 * Move the iterator to the next Record in the order, If this function was not previously called
 * after initialization by ipx_nf9_dset_iter_init(), then the iterator will point to the first
 * Record in the (Options) Template FlowSet.
 *
 * @code{.c}
 *   struct ipx_nf9_set_hdr *tmplt_set = ...;
 *   struct ipx_nf9_tset_iter it;
 *   ipx_nf9_tset_iter_init(&it, tmplt_set);
 *
 *   int rc;
 *   while ((rc = ipx_nf9_tset_iter_next(&it)) == IPX_OK) {
 *      // Add your code here...
 *   }
 *
 *   if (rc != IPX_EOC) {
 *      fprintf(stderr, "Error: %s\n", ipx_nf9_tset_iter_err(&it));
 *   }
 * @endcode
 * @param[in] it Pointer to the iterator
 * @return #IPX_OK on success and the Template Record is ready to use.
 * @return #IPX_EOC if no more Template Records are available (the end of the Set has been reached).
 * @return #IPX_ERR_FORMAT if the format of the Template FlowSet is invalid (an appropriate error
 *   message is set - see ipx_nf9_tset_iter_err()).
 */
int
ipx_nf9_tset_iter_next(struct ipx_nf9_tset_iter *it);

/**
 * @brief Get the last error message
 * @note The message is statically allocated string that can be passed to other function even
 *   when the iterator doesn't exist anymore.
 * @param[in] it Iterator
 * @return The error message
 */
const char *
ipx_nf9_tset_iter_err(const struct ipx_nf9_tset_iter *it);

#ifdef __cplusplus
}
#endif

#endif //IPFIXCOL2_NETFLOW9_PARSERS_H
