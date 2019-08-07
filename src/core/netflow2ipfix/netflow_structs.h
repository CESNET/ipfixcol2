/**
 * @file src/core/netflow2ipfix/netflow_structs.h
 * @author Lukas Hutak <lukas.hutak@cesnet.cz>
 * @brief NetFlow v5/v9 structures
 * @date 2018-2019
 *
 * Copyright(c) 2019 CESNET z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IPFIXCOL2_NETFLOW_H
#define IPFIXCOL2_NETFLOW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/**
 * @brief NetFlow v5 Packet Header structure
 * @warning All values are stored in Network Byte Order!
 */
struct __attribute__((__packed__)) ipx_nf5_hdr {
    /// NetFlow export format version number
    uint16_t version;
    /// Number of flows exported in this packet (1 - 30)
    uint16_t count;
    /// Current time in milliseconds since the export device booted
    uint32_t sys_uptime;
    /// Current count of seconds since 0000 UTC 1970
    uint32_t unix_sec;
    /// Residual nanoseconds since 0000 UTC 1970
    uint32_t unix_nsec;
    /// Sequence counter of total flows seen
    uint32_t flow_seq;
    /// Type of flow-switching engine
    uint8_t  engine_type;
    /// Slot number of the flow-switching engine
    uint8_t  engine_id;
    /// First two bits hold the sampling mode. Remaining 14 bits hold value of sampling interval
    uint16_t sampling_interval;
};

/**
 * @brief NetFlow v5 Record structure
 * @warning All values are stored in Network Byte Order!
 */
struct __attribute__((__packed__)) ipx_nf5_rec {
    /// Source IPv4 address
    uint32_t addr_src;
    /// Destination IPv4 address
    uint32_t addr_dst;
    /// IPv4 address of next hop router
    uint32_t nexthop;
    /// SNMP index of input interface
    uint16_t snmp_input;
    /// SNMP index of output interface
    uint16_t snmp_output;
    /// Packets in the flow
    uint32_t delta_pkts;
    /// Total number of Layer 3 bytes in the packets of the flow
    uint32_t delta_octets;
    /// SysUptime at start of flow
    uint32_t ts_first;
    /// SysUptime at the time the last packet of the flow was received
    uint32_t ts_last;
    /// TCP/UDP source port number or equivalent
    uint16_t port_src;
    /// TCP/UDP destination port number or equivalent
    uint16_t port_dst;
    /// Unused (zero) bytes
    uint8_t  _pad1;
    /// Cumulative OR of TCP flags
    uint8_t  tcp_flags;
    /// IP protocol type (for example, TCP = 6; UDP = 17)
    uint8_t  proto;
    /// IP type of service (ToS)
    uint8_t  tos;
    /// Autonomous system number of the source, either origin or peer
    uint16_t as_src;
    /// Autonomous system number of the destination, either origin or peer
    uint16_t as_dst;
    /// Source address prefix mask bits
    uint8_t  mask_src;
    /// Destination address prefix mask bits
    uint8_t  mask_dst;
    /// Unused (zero) bytes
    uint16_t _pad2;
};

/// NetFlow v5 version number
#define IPX_NF5_VERSION 0x5
/// Size of NetFlow v5 Packet Header
#define IPX_NF5_MSG_HDR_LEN sizeof(struct ipx_nf5_hdr)
/// Size of NetFlow v5 Packet Record
#define IPX_NF5_MSG_REC_LEN sizeof(struct ipx_nf5_rec)

// ------------------------------------------------------------------------------------------------

/**
 * @struct ipx_nf9_msg_hdr
 * @brief NetFlow v9 Packet Header structure
 * @warning All values are stored in Network Byte Order!
 */
struct __attribute__((__packed__)) ipx_nf9_msg_hdr {
    /** Version of Flow Record format exported in this packet                                    */
    uint16_t version;
    /** The total number of records in the Export Packet, which is the sum of Options FlowSet
     *  records, Template FlowSet records, and Data FlowSet records.
     */
    uint16_t count;
    /** Time in milliseconds since this device was first booted.                                 */
    uint32_t sys_uptime;
    /** Time in seconds since 0000 UTC 1970, at which the Export Packet leaves the Exporter.     */
    uint32_t unix_sec;
    /**
     * Incremental sequence counter of all Export Packets sent from the current Observation
     * Domain by the Exporter. This value MUST be cumulative, and SHOULD be used by the Collector
     * to identify whether any Export Packets have been missed.
     */
    uint32_t seq_number;
    /**
     * A 32-bit value that identifies the Exporter Observation Domain. NetFlow Collectors SHOULD
     * use the combination of the source IP address and the Source ID field to separate different
     * export streams originating from the same Exporter.
     */
    uint32_t source_id;
};

/// NetFlow v9 version number
#define IPX_NF9_VERSION 0x9
/// Size of NetFlow v5 Packet Header
#define IPX_NF9_MSG_HDR_LEN sizeof(struct ipx_nf9_msg_hdr)

/// Flowset type identifiers
enum ipx_nf9_set_id {
    IPX_NF9_SET_TMPLT       = 0,  ///< Template FlowSet ID
    IPX_NF9_SET_OPTS_TMPLT  = 1,  ///< Options Template FlowSet ID
    IPX_NF9_SET_MIN_DSET    = 256 ///< Minimum FlowSet ID for any Data FlowSet
};

/**
 * @struct ipx_nf9_set_hdr
 * @brief NetFlow v9 Set Header structure
 * @remark Based on RFC 3954, Section 5.1.
 * @warning All values are stored in Network Byte Order!
 */
struct __attribute__((__packed__)) ipx_nf9_set_hdr {
    /** FlowSet ID                                                                               */
    uint16_t flowset_id;
    /** Total length of this FlowSet.
     *
     * Because an individual FlowSet MAY contain multiple Records, the Length value MUST be used to
     * determine the position of the next FlowSet record, which could be any type of FlowSet.
     * Length is the sum of the lengths of the FlowSet ID, the Length itself, and all Records
     * within this FlowSet.
     */
    uint16_t length;
};

/// Length of NetFlow FlowSet header (in bytes)
#define IPX_NF9_SET_HDR_LEN sizeof(struct ipx_nf9_set_hdr)

/**
 * @struct ipx_nf9_tmplt_ie
 * @brief NetFlow v9 Field definition structure
 * @remark Based on RFC 3954, Section 5.2.
 * @warning All values are stored in Network Byte Order!
 */
struct __attribute__((__packed__)) ipx_nf9_tmplt_ie {
    /// A numeric value that represents the type of the field.
    uint16_t id;
    /// The length of the corresponding Field Type, in bytes.
    uint16_t length;
};

/** Length of NetFlow v9 Field definition structure */
#define IPX_NF9_TMPLT_IE_LEN sizeof(struct ipx_nf9_tmplt_ie)

/**
 * @struct ipx_nf9_trec
 * @brief NetFlow v9 Template record structure
 * @remark Based on RFC 3954, Section 5.2.
 * @warning All values are stored in Network Byte Order!
 */
struct __attribute__((__packed__)) ipx_nf9_trec {
    /**
     * @brief Template ID of this Template.
     *
     * Each of the newly generated Template Records is given a unique Template ID.
     * This uniqueness is local to the Observation Domain that generated the Template ID.
     * Template IDs 0-255 are reserved for Template FlowSets, Options FlowSets, and other
     * reserved FlowSets yet to be created.
     *
     * Template IDs of Data FlowSets are numbered from 256 (::IPX_NF9_SET_MIN_DSET) to 65535.
     */
    uint16_t template_id;
    /**
     * Number of fields in this Template Record. Because a Template FlowSet usually contains
     * multiple Template Records, this field allows the Collector to determine the end of the
     * current Template Record and the start of the next.
     */
    uint16_t count;
    /**
     * Field Specifier(s)
     */
    struct ipx_nf9_tmplt_ie fields[1];
};

/**
 * @struct ipx_nf9_tset
 * @brief NetFlow Template Set structure
 *
 * Consists of the common Set header and the first Template record.
 * @remark Based on RFC 3954, Section 5.2.
 * @warning All values are stored in Network Byte Order!
 */
struct __attribute__((__packed__)) ipx_nf9_tset {
    /**
     * Common NetFlow v9 FlowSet header.
     * Identification of the FlowSet MUST be 0 (::IPX_NF9_SET_TMPLT)
     */
    struct ipx_nf9_set_hdr header;

    /**
     * The first of template records in this Template FlowSet. Real size of the record is unknown
     * here due to a variable count of fields in each record.
     */
    struct ipx_nf9_trec first_record;
};

/**
 * @struct ipx_nf9_opts_trec
 * @brief NetFlow v9 Options Template record structure
 * @remark Based on RFC 3954, Section 6.1.
 * @warning All values are stored in Network Byte Order!
 */
struct __attribute__((__packed__)) ipx_nf9_opts_trec {
    /**
     * @brief Template ID of this Options Template.
     *
     * Each of the newly generated Template Records is given a unique Template ID.
     * This uniqueness is local to the Observation Domain that generated the Template ID.
     * Template IDs 0-255 are reserved for Template FlowSets, Options FlowSets, and other
     * reserved FlowSets yet to be created.
     *
     * Template IDs of Data FlowSets are numbered from 256 (::IPX_NF9_SET_MIN_DSET) to 65535.
     */
    uint16_t template_id;
    /**
     * The length in bytes of any Scope field definition contained in the Options Template
     * Record (The use of "Scope" is described below).
     */
    uint16_t scope_length;
    /**
     * The length (in bytes) of any options field definitions contained in this Options Template
     * Record.
     */
    uint16_t option_length;

    /**
     * Field Specifier(s)
     * @note The Scope fields always precede the Option fields.
     */
    struct ipx_nf9_tmplt_ie fields[1];
};

/**
 * @struct ipx_nf9_opts_tset
 * @brief NetFlow Template Set structure
 *
 * Consists of the common Set header and the first Options Template record.
 * @remark Based on RFC 3954, Section 6.1.
 * @warning All values are stored in Network Byte Order!
 */
struct __attribute__((__packed__)) ipx_nf9_opts_tset {
    /**
     * Common NetFlow v9 FlowSet header.
     * Identification of the FlowSet MUST be 1 (::IPX_NF9_SET_OPTS_TMPLT)
     */
    struct ipx_nf9_set_hdr header;

    /**
     * The first of template records in this Options Template FlowSet. Real size of the record is
     * unknown here due to a variable count of fields in each record.
     */
    struct ipx_nf9_opts_trec first_record;
};

/**
 * @struct ipx_nf9_dset
 * @brief NetFlow v9 Data FlowSet structure
 *
 * The Data Records are sent in Data Sets. It consists only of one or more Field Values. The
 * Template ID to which the Field Values belong is encoded in the Set Header field "Set ID", i.e.,
 * "Set ID" = "Template ID".
 */
struct __attribute__((__packed__)) ipx_nf9_dset {
    /**
     * Common NetFlow v9 FlowSet header.
     * Identification of the FlowSet MUST be at least 256 (::IPX_NF9_SET_MIN_DSET) and at
     * most 65535.
     */
    struct ipx_nf9_set_hdr header;

    /// Start of the first Data record
    uint8_t record[1];
};

#ifdef __cplusplus
}
#endif
#endif // IPFIXCOL2_NETFLOW_H
