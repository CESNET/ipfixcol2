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

/// Scope Field Type for a system
#define IPX_NF9_SCOPE_SYSTEM    1U
/// Scope Field Type for an interface
#define IPX_NF9_SCOPE_INTERFACE 2U
/// Scope Field Type for a line card
#define IPX_NF9_SCOPE_LINE_CARD 3U
/// Scope Field Type for cache
#define IPX_NF9_SCOPE_CACHE     4U
/// Scope Field Type for a template
#define IPX_NF9_SCOPE_TEMPLATE  5U


/// Incoming counter with length N x 8 bits for number of bytes associated with an IP Flow.
#define IPX_NF9_IE_IN_BYTES 1U
/// Incoming counter with length N x 8 bits for the number of packets associated with an IP Flow
#define IPX_NF9_IE_IN_PKTS 2U
/// Number of flows that were aggregated; default for N is 4
#define IPX_NF9_IE_FLOWS 3U
/// IP protocol byte
#define IPX_NF9_IE_PROTOCOL 4U
/// Type of Service byte setting when entering incoming interface
#define IPX_NF9_IE_SRC_TOS 5U
/// Cumulative of all the TCP flags seen for this flow
#define IPX_NF9_IE_TCP_FLAGS 6U
/// TCP/UDP source port number i.e.: FTP, Telnet, or equivalent
#define IPX_NF9_IE_L4_SRC_PORT 7U
/// IPv4 source address
#define IPX_NF9_IE_IPV4_SRC_ADDR 8U
/// The number of contiguous bits in the source address subnet mask
#define IPX_NF9_IE_SRC_MASK 9U
/// Input interface index; default for N is 2 but higher values could be used
#define IPX_NF9_IE_INPUT_SNMP 10U
/// TCP/UDP destination port number i.e.: FTP, Telnet, or equivalent
#define IPX_NF9_IE_L4_DST_PORT 11U
/// IPv4 destination address
#define IPX_NF9_IE_IPV4_DST_ADDR 12U
/// The number of contiguous bits in the destination address subnet mask
#define IPX_NF9_IE_DST_MASK 13U
/// Output interface index; default for N is 2 but higher values could be used
#define IPX_NF9_IE_OUTPUT_SNMP 14U
/// IPv4 address of next-hop router
#define IPX_NF9_IE_IPV4_NEXT_HOP 15U
/// Source BGP autonomous system number where N could be 2 or 4
#define IPX_NF9_IE_SRC_AS 16U
/// Destination BGP autonomous system number where N could be 2 or 4
#define IPX_NF9_IE_DST_AS 17U
/// Next-hop router's IP in the BGP domain
#define IPX_NF9_IE_BGP_IPV4_NEXT_HOP 18U
/// IP multicast outgoing packet counter with length N x 8 bits for packets associated with the IP Flow
#define IPX_NF9_IE_MUL_DST_PKTS 19U
/// IP multicast outgoing byte counter with length N x 8 bits for bytes associated with the IP Flow
#define IPX_NF9_IE_MUL_DST_BYTES 20U
/// System uptime at which the last packet of this flow was switched
#define IPX_NF9_IE_LAST_SWITCHED 21U
/// System uptime at which the first packet of this flow was switched
#define IPX_NF9_IE_FIRST_SWITCHED 22U
/// Outgoing counter with length N x 8 bits for the number of bytes associated with an IP Flow
#define IPX_NF9_IE_OUT_BYTES 23U
/// Outgoing counter with length N x 8 bits for the number of packets associated with an IP Flow.
#define IPX_NF9_IE_OUT_PKTS 24U
/// Minimum IP packet length on incoming packets of the flow
#define IPX_NF9_IE_MIN_PKT_LNGTH 25U
/// Maximum IP packet length on incoming packets of the flow
#define IPX_NF9_IE_MAX_PKT_LNGTH 26U
/// IPv6 Source Address
#define IPX_NF9_IE_IPV6_SRC_ADDR 27U
/// IPv6 Destination Address
#define IPX_NF9_IE_IPV6_DST_ADDR 28U
/// Length of the IPv6 source mask in contiguous bits
#define IPX_NF9_IE_IPV6_SRC_MASK 29U
/// Length of the IPv6 destination mask in contiguous bits
#define IPX_NF9_IE_IPV6_DST_MASK 30U
/// IPv6 flow label as per RFC 2460 definition
#define IPX_NF9_IE_IPV6_FLOW_LABEL 31U
/// Internet Control Message Protocol (ICMP) packet type; reported as ((ICMP Type*256) + ICMP code)
#define IPX_NF9_IE_ICMP_TYPE 32U
/// Internet Group Management Protocol (IGMP) packet type
#define IPX_NF9_IE_MUL_IGMP_TYPE 33U
/// When using sampled NetFlow, the rate at which packets are sampled
#define IPX_NF9_IE_SAMPLING_INTERVAL 34U
/// The type of algorithm used for sampled NetFlow: 0x01 Deterministic, 0x02 Random
#define IPX_NF9_IE_SAMPLING_ALGORITHM 35U
/// Timeout value (in seconds) for active flow entries in the NetFlow cache
#define IPX_NF9_IE_FLOW_ACTIVE_TIMEOUT 36U
/// Timeout value (in seconds) for inactive flow entries in the NetFlow cache
#define IPX_NF9_IE_FLOW_INACTIVE_TIMEOUT 37U
/// Type of flow switching engine: RP = 0, VIP/Linecard = 1
#define IPX_NF9_IE_ENGINE_TYPE 38U
/// ID number of the flow switching engine
#define IPX_NF9_IE_ENGINE_ID 39U
/// Counter with length N x 8 bits for bytes for the number of bytes exported by the Observation Domain
#define IPX_NF9_IE_TOTAL_BYTES_EXP 40U
/// Counter with length N x 8 bits for bytes for the number of packets exported by the Observation Domain
#define IPX_NF9_IE_TOTAL_PKTS_EXP 41U
/// Counter with length N x 8 bits for bytes for the number of flows exported by the Observation Domain
#define IPX_NF9_IE_TOTAL_FLOWS_EXP 42U
/// IPv4 source address prefix (specific for Catalyst architecture)
#define IPX_NF9_IE_IPV4_SRC_PREFIX 44U
/// IPv4 destination address prefix (specific for Catalyst architecture)
#define IPX_NF9_IE_IPV4_DST_PREFIX 45U
/// MPLS Top Label Type: 0x00 UNKNOWN 0x01 TE-MIDPT 0x02 ATOM 0x03 VPN 0x04 BGP 0x05 LDP
#define IPX_NF9_IE_MPLS_TOP_LABEL_TYPE 46U
/// Forwarding Equivalent Class corresponding to the MPLS Top Label
#define IPX_NF9_IE_MPLS_TOP_LABEL_IP_ADDR 47U
/// Identifier shown in "show flow-sampler"
#define IPX_NF9_IE_FLOW_SAMPLER_ID 48U
/// The type of algorithm used for sampling data: 0x02 random sampling.
#define IPX_NF9_IE_FLOW_SAMPLER_MODE 49U
/// Packet interval at which to sample.
#define IPX_NF9_IE_FLOW_SAMPLER_RANDOM_INTERVAL 50U
/// Minimum TTL on incoming packets of the flow
#define IPX_NF9_IE_MIN_TTL 52U
/// Maximum TTL on incoming packets of the flow
#define IPX_NF9_IE_MAX_TTL 53U
/// The IP v4 identification field
#define IPX_NF9_IE_IPV4_IDENT 54U
/// Type of Service byte setting when exiting outgoing interface
#define IPX_NF9_IE_DST_TOS 55U
/// Incoming source MAC address
#define IPX_NF9_IE_IN_SRC_MAC 56U
/// Outgoing destination MAC address
#define IPX_NF9_IE_OUT_DST_MAC 57U
/// Virtual LAN identifier associated with ingress interface
#define IPX_NF9_IE_SRC_VLAN 58U
/// Virtual LAN identifier associated with egress interface
#define IPX_NF9_IE_DST_VLAN 59U
/// Internet Protocol Version Set to 4 for IPv4, set to 6 for IPv6.
#define IPX_NF9_IE_IP_PROTOCOL_VERSION 60U
/// Flow direction: 0 - ingress flow, 1 - egress flow
#define IPX_NF9_IE_DIRECTION 61U
/// IPv6 address of the next-hop router
#define IPX_NF9_IE_IPV6_NEXT_HOP 62U
/// Next-hop router in the BGP domain
#define IPX_NF9_IE_BPG_IPV6_NEXT_HOP 63U
/// Bit-encoded field identifying IPv6 option headers found in the flow
#define IPX_NF9_IE_IPV6_OPTION_HEADERS 64U
/// MPLS label at position 1 in the stack.
#define IPX_NF9_IE_MPLS_LABEL_1 70U
/// MPLS label at position 2 in the stack.
#define IPX_NF9_IE_MPLS_LABEL_2 71U
/// MPLS label at position 3 in the stack.
#define IPX_NF9_IE_MPLS_LABEL_3 72U
/// MPLS label at position 4 in the stack.
#define IPX_NF9_IE_MPLS_LABEL_4 73U
/// MPLS label at position 5 in the stack.
#define IPX_NF9_IE_MPLS_LABEL_5 74U
/// MPLS label at position 6 in the stack.
#define IPX_NF9_IE_MPLS_LABEL_6 75U
/// MPLS label at position 7 in the stack.
#define IPX_NF9_IE_MPLS_LABEL_7 76U
/// MPLS label at position 8 in the stack.
#define IPX_NF9_IE_MPLS_LABEL_8 77U
/// MPLS label at position 9 in the stack.
#define IPX_NF9_IE_MPLS_LABEL_9 78U
/// MPLS label at position 10 in the stack.
#define IPX_NF9_IE_MPLS_LABEL_10 79U
/// Incoming destination MAC address
#define IPX_NF9_IE_IN_DST_MAC 80U
/// Outgoing source MAC address
#define IPX_NF9_IE_OUT_SRC_MAC 81U
/// Shortened interface name i.e.: "FE1/0"
#define IPX_NF9_IE_IF_NAME 82U
/// Full interface name i.e.: "'FastEthernet 1/0"
#define IPX_NF9_IE_IF_DESC 83U
/// Name of the flow sampler
#define IPX_NF9_IE_SAMPLER_NAME 84U
/// Running byte counter for a permanent flow
#define IPX_NF9_IE_IN_PERMANENT_BYTES 85U
/// Running packet counter for a permanent flow
#define IPX_NF9_IE_IN_PERMANENT_PKTS 86U
/// The fragment-offset value from fragmented IP packets
#define IPX_NF9_IE_FRAGMENT_OFFSET 88U
/// Forwarding status is encoded on 1 byte (2 left bits status, 6 remaining bits reason code)
#define IPX_NF9_IE_FORWARDING_STATUS 89U
/// MPLS PAL Route Distinguisher.
#define IPX_NF9_IE_MPLS_PAL_RD 90U
/// Number of consecutive bits in the MPLS prefix length.
#define IPX_NF9_IE_MPLS_PREFIX_LEN 91U
/// BGP Policy Accounting Source Traffic Index
#define IPX_NF9_IE_SRC_TRAFFIC_INDEX 92U
/// BGP Policy Accounting Destination Traffic Index
#define IPX_NF9_IE_DST_TRAFFIC_INDEX 93U
/// Application description.
#define IPX_NF9_IE_APPLICATION_DESCRIPTION 94U
/// 8 bits of engine ID, followed by n bits of classification.
#define IPX_NF9_IE_APPLICATION_TAG 95U
/// Name associated with a classification.
#define IPX_NF9_IE_APPLICATION_NAME 96U
/// The value of a Differentiated Services Code Point (DSCP)
#define IPX_NF9_IE_POSTIP_DIFF_SERV_CODE_POINT 98U
/// Multicast replication factor.
#define IPX_NF9_IE_REPLICATION_FACTOR 99U
/// Layer 2 packet section offset. Potentially a generic offset.
#define IPX_NF9_IE_L2_PACKET_SECTION_OFFSET 102U
/// Layer 2 packet section size. Potentially a generic size.
#define IPX_NF9_IE_L2_PACKET_SECTION_SIZE 103U
/// Layer 2 packet section data.
#define IPX_NF9_IE_L2_PACKET_SECTION_DATA 104U

#ifdef __cplusplus
}
#endif
#endif // IPFIXCOL2_NETFLOW_H
