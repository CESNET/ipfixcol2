#!/usr/bin/env python3

"""
Simple tool for replaying NetFlow v5/v9 and IPFIX packets to a collector.

Author(s):
    Lukas Hutak <lukas.hutak@cesnet.cz>
Date: July 2019

Copyright(c) 2019 CESNET z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause
"""

import argparse
import struct
from scapy.all import *

# Dictionary with Transport Sessions
sessions = {}

def process_pcap():
    """
    Open PCAP and send each NetFlow/IPFIX packet to a collector.
    :return: None
    """
    # Try to open the file
    reader = PcapReader(args.file)

    cnt_total = 0
    cnt_sent = 0

    for pkt in reader:
        cnt_total += 1
        if args.verbose:
            print("Processing {}. packet".format(cnt_total))
        if process_packet(pkt):
            cnt_sent += 1

    print("{} of {} packets have been processed and sent "
        "over {} Transport Session(s)!".format(cnt_sent, cnt_total, len(sessions)))


def process_packet(pkt):
    """
    Extract NetFlow v5/v9 or IPFIX payload and send it as a new packet to a collector.

    :param pkt: Scapy packet to process and send
    :return: True if the packet has been send to the destination. False otherwise.
    :rtype: bool
    """
    # Determine IP adresses
    l3_data = pkt.getlayer("IP")
    if not l3_data:
        print("Unable to locate L3 layer. Skipping...")
    ip_src = l3_data.src
    ip_dst = l3_data.dst

    # Determine protocol of Transport Layer
    l4_data = None
    proto = None
    port_src = None
    port_dst = None

    for type in ["UDP", "TCP"]:
        if not l3_data.haslayer(type):
            continue
        l4_data = l3_data.getlayer(type)
        proto = type
        port_src = l4_data.sport
        port_dst = l4_data.dport
        break

    if not proto:
        if args.verbose:
            print("Failed to locate L4 layer. Skipping...")
        return False

    # Check if the packet contains NetFlow v5/v9 or IPFIX payload
    l7_data = l4_data.payload
    raw_payload = l7_data.original
    version = struct.unpack("!H", raw_payload[:2])[0]
    if version not in [5, 9, 10]:
        print("Payload doesn't contain NetFlow/IPFIX packet. Skipping...")
        return False

    # Send the packet
    key = (ip_src, ip_dst, proto, port_src, port_dst)
    send_packet(key, raw_payload)
    return True


def send_packet(key, payload):
    """
    Send packet to a collector.

    To make sure that packets from different Transport Session (TS) are not mixed together,
    the function creates and maintains independent UDP/TCP session for each original TS.

    :param key:   Identification of the original Transport Session
                  (src IP, dst IP, proto, src port, dst port)
    :param payload: Raw NetFlow/IPFIX message to send
    :return: None
    """
    ts = sessions.get(key)
    if not ts:
        # Create a new Transport Session
        proto = key[2]

        if args.verbose:
            print("Creating a new Transport Session for {}".format(key))
        if args.proto != proto:
            print("WARNING: Original flow packets exported over {proto_orig} "
                  "({src_ip}:{src_port} -> {dst_ip}:{dst_port}) are now being send over {proto_now}. "
                  "Collector could reject these flows due to different formatting rules!".format(
                    proto_orig=proto, proto_now=args.proto, src_ip=key[0], dst_ip=key[1], src_port=key[3],
                    dst_port=key[4]))

        ts = create_socket()
        sessions[key] = ts

    # Send the packet
    ts.sendall(payload)


def create_socket():
    """
    Create a new socket and connect it to the collector.

    :return: Socket
    :rtype: socket.socket
    """
    str2proto = {
        "UDP": socket.SOCK_DGRAM,
        "TCP": socket.SOCK_STREAM
    }

    family = socket.AF_UNSPEC
    if args.v4_only:
        family = socket.AF_INET
    if args.v6_only:
        family = socket.AF_INET6

    net_proto = str2proto[args.proto]
    s = None

    for res in socket.getaddrinfo(args.addr, args.port, family, net_proto):
        net_af, net_type, net_proto, net_cname, net_sa = res
        try:
            s = socket.socket(net_af, net_type, net_proto)
        except socket.error as err:
            s = None
            continue

        try:
            s.connect(net_sa)
        except socket.error as err:
            s.close()
            s = None
            continue
        break

    if s is None:
        raise RuntimeError("Failed to open socket!")
    return s

def arg_check_port(value):
    """
    Check if port is valid number
    :param str value: String to convert
    :return: Port
    """
    num = int(value)
    if num < 0 or num >= 2**16:
        raise argparse.ArgumentTypeError("%s is not valid port number" % value)
    return num

if __name__ == "__main__":
    # Parse arguments
    parser = argparse.ArgumentParser(
        description="Simple tool for replaying NetFlow v5/v9 and IPFIX packets to a collector.",
    )
    parser.add_argument("-i", dest="file",    help="PCAP with NetFlow/IPFIX packets", required=True)
    parser.add_argument("-d", dest="addr",    help="Destination IP address or hostname (default: %(default)s)",
        default="127.0.0.1")
    parser.add_argument("-p", dest="port",    help="Destination port number (default: %(default)d)",
        default=4739, type=arg_check_port)
    parser.add_argument("-t", dest="proto",   help="Connection type (default: %(default)s)",
        default="UDP", choices=["UDP", "TCP"])
    parser.add_argument("-v", dest="verbose", help="Increase verbosity", default=False, action="store_true")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("-4", dest="v4_only",  help="Force the tool to send flows to an IPv4 address only",
        default=False, action="store_true")
    group.add_argument("-6", dest="v6_only",  help="Force the tool to send flows to an IPv6 address only",
        default=False, action="store_true")
    args = parser.parse_args()

    # Process the PCAP file
    try:
        process_pcap()
    except Exception as err:
        print("ERROR: {}".format(err))
