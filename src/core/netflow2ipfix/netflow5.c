/**
 * @file src/core/netflow2ipfix/netflow5.c
 * @author Lukas Hutak <lukas.hutak@cesnet.cz>
 * @brief Converter from NetFlow v5 to IPFIX Message (source code)
 * @date 2018-2019
 *
 * Copyright(c) 2019 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdbool.h>
#include <stdint.h>
#include <endian.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>

#include <ipfixcol2.h>
#include <libfds.h>
#include "netflow_structs.h"
#include "netflow2ipfix.h"
#include "../message_ipfix.h"
#include "../verbose.h"

// Simple static asserts to prevent unexpected structure modifications!
static_assert(IPX_NF5_MSG_HDR_LEN == 24U, "NetFlow v5 header size is not valid!");
static_assert(IPX_NF5_MSG_REC_LEN == 48U, "NetFlow v5 record size is not valid!");

/// Auxiliary definition to represent size of 1 byte
#define BYTES_1 (1U)
/// Auxiliary definition to represent size of 2 bytes
#define BYTES_2 (2U)
/// Auxiliary definition to represent size of 4 bytes
#define BYTES_4 (4U)
/// Auxiliary definition to represent size of 8 bytes
#define BYTES_8 (8U)

/**
 * @brief IPFIX Template Set of modified NetFlow v5 record
 *
 * The set consists of Template Set header and a single Template definition.
 * @note
 *   All values are in "host byte order" and MUST be converted to "network byte order" before
 *   they can be used!
 * @warning
 *   "first" and "last" timestamps (relative to exporter time SysUpTime) are replaced with
 *   absolute timestamps in milliseconds!
 */
static const uint16_t nf5_tmpl_set[] = {
    // IPFIX Set header + IPFIX Template header (ID 256)
    FDS_IPFIX_SET_TMPLT,    0, // Size and field count will be filled later
    FDS_IPFIX_SET_MIN_DSET, 0,
    // Template fields
    8U,   BYTES_4, // iana:sourceIPv4Address
    12U,  BYTES_4, // iana:destinationIPv4Address
    15U,  BYTES_4, // iana:ipNextHopIPv4Address
    10U,  BYTES_2, // iana:ingressInterface
    14U,  BYTES_2, // iana:egressInterface
    2U,   BYTES_4, // iana:packetDeltaCount
    1U,   BYTES_4, // iana:octetDeltaCount
    152U, BYTES_8, // iana:flowStartMilliseconds (absolute timestamp in milliseconds)
    153U, BYTES_8, // iana:flowEndMilliseconds   (absolute timestamp in milliseconds)
    7U,   BYTES_2, // iana:sourceTransportPort
    11U,  BYTES_2, // iana:destinationTransportPort
    210U, BYTES_1, // iana:paddingOctets
    6U,   BYTES_1, // iana:tcpControlBits
    4U,   BYTES_1, // iana:protocolIdentifier
    5U,   BYTES_1, // iana:ipClassOfService
    16U,  BYTES_2, // iana:bgpSourceAsNumber
    17U,  BYTES_2, // iana:bgpDestinationAsNumber
    9U,   BYTES_1, // iana:sourceIPv4PrefixLength
    13U,  BYTES_1, // iana:destinationIPv4PrefixLength
    35U,  BYTES_1, // iana:samplingAlgorithm
    210U, BYTES_1, // iana:paddingOctets
    34U,  BYTES_4  // iana:samplingInterval
};

/// Number of fields (including header fields) in the Template Set
#define NF5_TSET_ITEMS (sizeof(nf5_tmpl_set) / sizeof(nf5_tmpl_set[0]))
static_assert(NF5_TSET_ITEMS % 2 == 0, "Number of fields MUST be even!");

/**
 * @brief Structure of a NetFlow v5 record after convertion to IPFIX
 * @note The structure MUST match IPFIX Template ::nf5_tmpl_set
 */
struct __attribute__((__packed__)) new_ipx_rec {
    uint32_t addr_src;      ///< Source IPv4 address
    uint32_t addr_dst;      ///< Destination IPv4 address
    uint32_t nexthop;       ///< IPv4 address of next hop router
    uint16_t snmp_input;    ///< SNMP index of input interface
    uint16_t snmp_output;   ///< SNMP index of output interface
    uint32_t delta_pkts;    ///< Packets in the flow
    uint32_t delta_octets;  ///< Total number of Layer 3 bytes in the packets of the flow
    uint64_t ts_first;      ///< Absolute timestamp of the first packet (in milliseconds)
    uint64_t ts_last;       ///< Absolute timestamp of the last packet (in milliseconds)
    uint16_t port_src;      ///< TCP/UDP source port number or equivalent
    uint16_t port_dst;      ///< TCP/UDP destination port number or equivalent
    uint8_t  _pad1;         ///< Unused (zero) bytes
    uint8_t  tcp_flags;     ///< Cumulative OR of TCP flags
    uint8_t  proto;         ///< IP protocol type (for example, TCP = 6; UDP = 17)
    uint8_t  tos;           ///< IP type of service (ToS)
    uint16_t as_src;        ///< Autonomous system number of the source, either origin or peer
    uint16_t as_dst;        ///< Autonomous system number of the destination, either origin or peer
    uint8_t  mask_src;      ///< Source address prefix mask bits
    uint8_t  mask_dst;      ///< Destination address prefix mask bits
    uint8_t  sampling_alg;  ///< Sampling algorithm
    uint8_t  _pad2;         ///< Unused (zero) bytes
    uint32_t sampling_int;  ///< Sampling interval
};

/// Offset of "first" timestamp in the new IPFIX Data Record
#define IPX_FIRST_OFFSET (offsetof(struct new_ipx_rec, ts_first))
/// Offset of "last" timestamp in the new IPFIX Data Record
#define IPX_LAST_OFFSET  (offsetof(struct new_ipx_rec, ts_last))
/// Offset of "sampling" information in the new IPFIX Data Record
#define IPX_SAMPLING_OFFSET (offsetof(struct new_ipx_rec, sampling_alg))

/// Start position of the first part to copy in the original NetFlow v5 Message
#define PART1_NF_POS(x)  ((uint8_t *) (x))
/// Start position of the first part to copy in the new IPFIX Message
#define PART1_IPX_POS(x) ((uint8_t *) (x))
/// Size of the first part to copy
#define PART1_LEN        (offsetof(struct ipx_nf5_rec, ts_first))

/// Start position of the second part to copy in the original NetFlow v5 Message
#define PART2_NF_POS(x)  (((uint8_t *) (x)) + offsetof(struct ipx_nf5_rec, port_src))
/// Start position of the second part to copy in the new IPFIX Message
#define PART2_IPX_POS(x) (((uint8_t *) (x)) + offsetof(struct new_ipx_rec, port_src))
/// Size of the second part to copy (without tailing padding)
#define PART2_LEN (offsetof(struct ipx_nf5_rec, _pad2) - offsetof(struct ipx_nf5_rec, port_src))

static_assert(PART1_LEN == offsetof(struct new_ipx_rec, ts_first), "Different Part 1 size");
static_assert(PART2_LEN ==
    offsetof(struct new_ipx_rec, sampling_alg) - offsetof(struct new_ipx_rec, port_src),
    "Different Part 2 size");

/**
 * @def CONV_ERROR
 * @brief Macro for printing an error message of a converter
 * @param[in] conv    Converter
 * @param[in] fmt     Format string (see manual page for "printf" family)
 * @param[in] ...     Variable number of arguments for the format string
 */
#define CONV_ERROR(conv, fmt, ...)                                                \
    if ((conv)->conf.vlevel >= IPX_VERB_ERROR) {                                  \
        ipx_verb_print(IPX_VERB_ERROR, "ERROR: %s: [%s] " fmt "\n",               \
            (conv)->conf.ident, (conv)->msg_ctx->session->ident, ## __VA_ARGS__); \
    }

/**
 * @def CONV_WARNING
 * @brief Macro for printing a warning message of a converter
 * @param[in] conv    Converter
 * @param[in] fmt     Format string (see manual page for "printf" family)
 * @param[in] ...     Variable number of arguments for the format string
 */
#define CONV_WARNING(conv, fmt, ...)                                              \
    if ((conv)->conf.vlevel >= IPX_VERB_WARNING) {                                \
        ipx_verb_print(IPX_VERB_WARNING, "WARNING: %s: [%s] " fmt "\n",           \
            (conv)->conf.ident, (conv)->msg_ctx->session->ident, ## __VA_ARGS__); \
    }

/**
 * @def CONV_INFO
 * @brief Macro for printing an info message of a converter
 * @param[in] conv    Converter
 * @param[in] fmt     Format string (see manual page for "printf" family)
 * @param[in] ...     Variable number of arguments for the format string
 */
#define CONV_INFO(conv, fmt, ...)                                                 \
    if ((conv)->conf.vlevel >= IPX_VERB_INFO) {                                   \
        ipx_verb_print(IPX_VERB_INFO, "INFO: %s: [%s] " fmt "\n",                 \
            (conv)->conf.ident, (conv)->msg_ctx->session->ident, ## __VA_ARGS__); \
    }

/**
 * @def CONV_DEBUG
 * @brief Macro for printing a debug message of a converter
 * @param[in] conv    Converter
 * @param[in] fmt     Format string (see manual page for "printf" family)
 * @param[in] ...     Variable number of arguments for the format string
 */
#define CONV_DEBUG(conv, fmt, ...)                                                \
    if ((conv)->conf.vlevel >= IPX_VERB_DEBUG) {                                  \
        ipx_verb_print(IPX_VERB_DEBUG, "DEBUG: %s: [%s] " fmt "\n",               \
            (conv)->conf.ident, (conv)->msg_ctx->session->ident, ## __VA_ARGS__); \
    }

/// Internal converter structure
struct ipx_nf5_conv {
    /// Message context of a NetFlow message that is converted
    const struct ipx_msg_ctx *msg_ctx;

    struct {
        /// Expected sequence number of the next NetFlow message
        uint32_t next_nf;
        /// Next sequence number of the next IPFIX Message
        uint32_t next_ipx;
    } seq; ///< Sequence numbers

    struct {
        /// Instance identification (only for log!)
        char *ident;
        /// Verbosity level
        enum ipx_verb_level vlevel;
        /// Template refresh interval (in seconds)
        uint32_t refresh;
        /// Observation Domain ID of IPFIX message
        uint32_t odid;
    } conf; ///< Configuration parameters

    struct {
        /// Has the template been already sent?
        bool added;
        /** Timestamp of the next template refresh (must be enabled in conf.)
         *  If the "added" is false, this value is undefined! */
        uint32_t next_refresh;

        /// Template Set data (in "network byte order")
        uint16_t *tset_data;
        /// Template Set size
        size_t tset_size;
        /// Size of converted data record
        size_t drec_size;
    } tmplt; ///< Template information
};


ipx_nf5_conv_t *
ipx_nf5_conv_init(const char *ident, enum ipx_verb_level vlevel, uint32_t tmplt_refresh,
    uint32_t odid)
{
    struct ipx_nf5_conv *res = calloc(1, sizeof(*res));
    if (!res) {
        return NULL;
    }

    res->conf.ident = strdup(ident);
    if (!res->conf.ident) {
        free(res);
        return NULL;
    }

    // Prepare Template Set and calculate required parameters
    size_t tset_size = sizeof(nf5_tmpl_set);
    uint16_t *tset_data = malloc(tset_size);
    if (!tset_data) {
        free(res->conf.ident);
        free(res);
        return NULL;
    }


    uint16_t drec_size = 0;
    uint16_t field_cnt = 0;

    for (size_t i = 0; i < NF5_TSET_ITEMS; ++i) {
        // Convert fields from "host byte order" to "network byte order"
        tset_data[i] = htons(nf5_tmpl_set[i]);

        if (i >= 4 && i % 2 == 1) { // Ignore Set and Template header i.e. ">= 4"
            field_cnt++;
            drec_size += nf5_tmpl_set[i];
        }
    }
    // Update size and field count
    struct fds_ipfix_tset *tset_raw = (struct fds_ipfix_tset *) tset_data;
    tset_raw->header.length = htons((uint16_t) tset_size);
    tset_raw->first_record.count = htons(field_cnt);

    res->tmplt.tset_data = tset_data;
    res->tmplt.tset_size = tset_size;
    res->tmplt.drec_size = drec_size;
    res->tmplt.added = false;

    // Initialize remaining parameters
    res->conf.refresh = tmplt_refresh;
    res->conf.odid = odid;
    res->conf.vlevel = vlevel;
    return res;
}

void
ipx_nf5_conv_destroy(ipx_nf5_conv_t *conv)
{
    free(conv->conf.ident);
    free(conv->tmplt.tset_data);
    free(conv);
}

/**
 * @brief Calculate required size of the new IPFIX Message
 *
 * The new IPFIX Message will consists of IPFIX Message header (mandatory), predefined
 *   IPFIX Template Set (optional) and IPFIX Data Set (optional)
 * @param[in] conv    Converter internals
 * @param[in] rec_cnt Number of data records
 * @param[in] tmplt   Add IPFIX Template Set with the Template
 * @return Total size of the new IPFIX Message (in bytes)
 */
static inline size_t
conv_new_size(const ipx_nf5_conv_t *conv, uint16_t rec_cnt, bool tmplt)
{
    // IPFIX Message header
    size_t ret = FDS_IPFIX_MSG_HDR_LEN;

    if (tmplt) {
        // Add IPFIX Template Set (with 1 template)
        ret += conv->tmplt.tset_size;
    }

    if (rec_cnt > 0) {
        // Add IPFIX Data Set header + records based on the template
        ret += FDS_IPFIX_SET_HDR_LEN;
        ret += (rec_cnt * conv->tmplt.drec_size);
    }
    return ret;
}

/**
 * @brief Compare message timestamps (with timestamp wraparound support)
 * @param[in] t1 First timestamp
 * @param[in] t2 Second timestamp
 * @return  The  function  returns an integer less than, equal to, or greater than zero if the
 *   first timestamp \p t1 is found, respectively, to be less than, to match, or be greater than
 *   the second timestamp.
 */
static inline int
conv_time_cmp(uint32_t t1, uint32_t t2)
{
    if (t1 == t2) {
        return 0;
    }

    if ((t1 - t2) & 0x80000000) { // test the "sign" bit
        return (-1);
    } else {
        return 1;
    }
}

/**
 * @brief Add the IPFIX Message header
 *
 * @param[in] nf_msg   Original NetFlow v5 message
 * @param[in] ipx_data Pointer to the place where the new IPFIX header will be placed
 * @param[in] ipx_size Total size of the new IPFIX Message to be set
 * @param[in] ipx_odid Observation Domain ID (ODID) to be set
 * @param[in] ipx_seq  Sequence number to be set
 * @return Pointer to the memory right behind the added header
 */
static inline uint8_t *
conv_add_hdr(const uint8_t *nf_msg, uint8_t *ipx_data, uint16_t ipx_size, uint32_t ipx_odid,
    uint32_t ipx_seq)
{
    const struct ipx_nf5_hdr *nf5_hdr = (const struct ipx_nf5_hdr *) nf_msg;
    struct fds_ipfix_msg_hdr *ipx_hdr = (struct fds_ipfix_msg_hdr *) ipx_data;

    ipx_hdr->version = htons(FDS_IPFIX_VERSION);
    ipx_hdr->length = htons(ipx_size);
    ipx_hdr->export_time = nf5_hdr->unix_sec;
    ipx_hdr->seq_num = htonl(ipx_seq);
    ipx_hdr->odid = htonl(ipx_odid);

    return (ipx_data + FDS_IPFIX_MSG_HDR_LEN);
}

/**
 * @brief Add the predefined IPFIX Template Set
 *
 * @param[in] conv     Converter internals
 * @param[in] ipx_data Pointer to the place where the new IPFIX Template Set will be placed
 * @return Pointer to the memory right behind the added Template Set
 */
static inline uint8_t *
conv_add_tset(const ipx_nf5_conv_t *conv, uint8_t *ipx_data)
{
    memcpy(ipx_data, conv->tmplt.tset_data, conv->tmplt.tset_size);
    return (ipx_data + conv->tmplt.tset_size);
}

/**
 * @brief Add the IPFIX Data Set with converted records
 *
 * The function will add the Data Set header and all NetFlow records are converted to modified
 * IPFIX records, where relative timestamps are replaced with absolute ones
 *
 * @note
 *   If the NetFlow record doesn't contain any data record, nothing is added.
 * @param[in] conv     Converter internals
 * @param[in] nf_msg   Original NetFlow v5 message
 * @param[in] ipx_data Pointer to the place where the new IPFIX Data Set will be placed
 * @return Pointer to the memory right behind the added Data Set
 */
static inline uint8_t  *
conv_add_dset(const ipx_nf5_conv_t *conv, const uint8_t *nf_msg, uint8_t *ipx_data)
{
    const struct ipx_nf5_hdr *nf_hdr = (const struct ipx_nf5_hdr *) nf_msg;
    const uint16_t rec_cnt = ntohs(nf_hdr->count);

    if (rec_cnt == 0) {
        // Nothing to add
        return ipx_data;
    }

    // Prepare for timestamps conversion
    const uint64_t hdr_time_sec = ntohl(nf_hdr->unix_sec);
    const uint64_t hdr_time_nsec = ntohl(nf_hdr->unix_nsec);
    const uint64_t hdr_exp_time = (hdr_time_sec * 1000U) + (hdr_time_nsec / 1000000U);
    const uint64_t hdr_sys_time = ntohl(nf_hdr->sys_uptime);

    if (hdr_time_nsec >= 1000000000ULL) {
        CONV_WARNING(conv, "Unexpected number of nanoseconds in the message header (>= 10^9). "
            "Timestamps of some flows might not be accurate.", '\0');
    }

    // Prepare sampling information
    struct __attribute__((__packed__)) sampling_info {
        uint8_t alg;       ///< Sampling algorithm
        uint8_t _pad;      ///< Padding
        uint32_t interval; ///< Sampling interval
    } sinfo;

    const uint16_t sampling = ntohs(nf_hdr->sampling_interval);
    sinfo.alg = sampling >> 14U; // Only first 2 bits
    sinfo._pad = 0;
    sinfo.interval = htonl(sampling & 0x3FFF); // Remaining 14 bits

    // Add IPFIX Data Set header
    struct fds_ipfix_dset *ipx_dset = (struct fds_ipfix_dset *) ipx_data;
    const size_t dset_len = FDS_IPFIX_SET_HDR_LEN + (rec_cnt * conv->tmplt.drec_size);
    ipx_dset->header.flowset_id = htons(FDS_IPFIX_SET_MIN_DSET);
    ipx_dset->header.length = htons((uint16_t) dset_len);

    // Add all data records
    uint8_t *ipx_rec = &ipx_dset->records[0];
    for (uint16_t i = 0; i < rec_cnt; ++i, ipx_rec += conv->tmplt.drec_size) {
        const struct ipx_nf5_rec *nf_rec = (const struct ipx_nf5_rec *)
            (nf_msg + IPX_NF5_MSG_HDR_LEN + (i * IPX_NF5_MSG_REC_LEN));
        // New timestamps (in milliseconds)
        uint64_t ts_start = hdr_exp_time - (hdr_sys_time - ntohl(nf_rec->ts_first));
        uint64_t ts_end =   hdr_exp_time - (hdr_sys_time - ntohl(nf_rec->ts_last));
        uint64_t *ptr_first = (uint64_t *) (ipx_rec + IPX_FIRST_OFFSET);
        uint64_t *ptr_last  = (uint64_t *) (ipx_rec + IPX_LAST_OFFSET);
        struct sampling_info *ptr_sampling = (struct sampling_info *) (ipx_rec + IPX_SAMPLING_OFFSET);

        // Copy and extend the message record
        memcpy(PART1_IPX_POS(ipx_rec), PART1_NF_POS(nf_rec), PART1_LEN);
        *ptr_first = htobe64(ts_start);
        *ptr_last  = htobe64(ts_end);
        memcpy(PART2_IPX_POS(ipx_rec), PART2_NF_POS(nf_rec), PART2_LEN);
        *ptr_sampling = sinfo;
    }

    return (ipx_data + dset_len);
}

int
ipx_nf5_conv_process(ipx_nf5_conv_t *conv, ipx_msg_ipfix_t *wrapper)
{
    const struct ipx_nf5_hdr *nf5_hdr = (const struct ipx_nf5_hdr *) wrapper->raw_pkt;
    const size_t nf5_size = wrapper->raw_size;
    conv->msg_ctx = &wrapper->ctx;

    // Check the header and expected message size
    if (nf5_size < IPX_NF5_MSG_HDR_LEN) {
        CONV_ERROR(conv, "Length of NetFlow v5 Message is smaller than its header size!", '\0');
        return IPX_ERR_FORMAT;
    }

    if (ntohs(nf5_hdr->version) != IPX_NF5_VERSION) {
        CONV_ERROR(conv, "Invalid version number of NetFlow v5 Message (expected 5)", '\0');
        return IPX_ERR_FORMAT;
    }

    const uint16_t nf5_rec_cnt = ntohs(nf5_hdr->count);
    const uint32_t nf5_rec_time = ntohl(nf5_hdr->unix_sec);
    const uint32_t nf5_msg_seq = ntohl(nf5_hdr->flow_seq);
    if (!conv->tmplt.added) {
        // Copy sequence number of the first packet in the stream
        conv->seq.next_nf = nf5_msg_seq;
        conv->seq.next_ipx = nf5_msg_seq;
    }

    CONV_DEBUG(conv, "Converting a NetFlow Message v5 (seq. num. %" PRIu32") to an IPFIX Message "
        "(new seq. num. %" PRIu32 ")", nf5_msg_seq, conv->seq.next_ipx);

    // Check sequence numbers
    if (conv->seq.next_nf != nf5_msg_seq) {
        CONV_WARNING(conv, "Unexpected Sequence number (expected: %" PRIu32 ", got: %" PRId32 ")",
            conv->seq.next_nf, nf5_msg_seq);
        if (conv_time_cmp(nf5_msg_seq, conv->seq.next_nf) > 0) {
            // Message sequence number is greater than expected one... we lost one or more packets
            conv->seq.next_nf = nf5_msg_seq + nf5_rec_cnt;
        }
    } else {
        conv->seq.next_nf += nf5_rec_cnt;
    }

    if (IPX_NF5_MSG_HDR_LEN + (nf5_rec_cnt * IPX_NF5_MSG_REC_LEN) != nf5_size) {
        CONV_ERROR(conv, "Length of NetFlow v5 Message doesn't match with the number of records "
            "specified in the header.", '\0');
        return IPX_ERR_FORMAT;
    }

    // Create a new IPFIX Message
    bool add_tset = !conv->tmplt.added
        || (conv->conf.refresh != 0 && conv_time_cmp(nf5_rec_time, conv->tmplt.next_refresh) >= 0);

    const size_t ipx_size = conv_new_size(conv, nf5_rec_cnt, add_tset);
    if (ipx_size > UINT16_MAX) {
        /* Typically NetFlow messages SHOULD not have more that 30 record (due to UDP MTU), however,
         * we support as many records per message as possible
         */
        CONV_ERROR(conv, "Unable to convert NetFlow v5 to IPFIX. Size of the converted message "
            "exceeds the maximum size of an IPFIX Message! (before: %" PRIu16 " B, after: %zu B, "
            "limit: 65535)",
            nf5_size, ipx_size);
        return IPX_ERR_FORMAT;
    }

    const uint8_t *nf5_msg = wrapper->raw_pkt;
    uint8_t *ipx_msg = malloc(ipx_size * sizeof(uint8_t));
    if (!ipx_msg) {
        CONV_ERROR(conv, "A memory allocation failed (%s:%d).", __FILE__, __LINE__);
        return IPX_ERR_NOMEM;
    }

    // Fill the IPFIX Message
    uint8_t *next_set = ipx_msg;
    next_set = conv_add_hdr(nf5_msg, next_set, (uint16_t) ipx_size, conv->conf.odid, conv->seq.next_ipx);

    if (add_tset) {
        CONV_DEBUG(conv, "Adding a Template Set into the converted NetFlow Message.", '\0');
        next_set = conv_add_tset(conv, next_set);
        conv->tmplt.added = true;
        conv->tmplt.next_refresh = nf5_rec_time + conv->conf.refresh;
    }
    next_set = conv_add_dset(conv, nf5_msg, next_set);
    conv->seq.next_ipx += nf5_rec_cnt;

    // Finally, replace the converted NetFlow Message with the new IPFIX Message
    assert(next_set == (ipx_msg + ipx_size));
    free(wrapper->raw_pkt);
    wrapper->raw_pkt = ipx_msg;
    wrapper->raw_size = (uint16_t) ipx_size;
    return IPX_OK;
}

void
ipx_nf5_conv_verb(ipx_nf5_conv_t *conv, enum ipx_verb_level v_new)
{
    conv->conf.vlevel = v_new;
}