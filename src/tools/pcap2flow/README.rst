pcap2flow
=========

Simple tool for extracting NetFlow v5/v9 and IPFIX packets from a PCAP file and
exporting them to a collector.

The PCAP file can contain capture of multiple sessions between exporters and
one or more collectors. To preserve original Transport Sessions (and avoid
mixing packets together), the tool creates a new independent session to the
newly specified collector for each original TS.

All non-flow packets in the file are automatically ignored.

Dependencies
------------

- python3
- `python3-scapy <https://pypi.org/project/scapy/>`_

Parameters
----------

:``-h``:
    Show help message and exit
:``-i FILE``:
    PCAP file with NetFlow/IPFIX packets
:``-d ADDR``:
    Destination IP address or hostname (default: 127.0.0.1)
:``-p PORT``:
    Destination port number (default: 4739)
:``-t TYPE``:
    Connection type (options: TCP/UDP, default: UDP)
:``-4``:
    Force the tool to send flows to an IPv4 address only
:``-6``:
    Force the tool to send flows to an IPv6 address only
:``-v``:
    Increase verbosity

Examples
--------

Replay packets over UDP to a collector listening on localhost (port 4739):

.. code:: bash

    ./pcap2flow -i data.pcap

Replay packets over TCP to a collector on example.org (port 3000):

.. code:: bash

    ./pcap2flow -i data.pcap -d example.org -p 3000 -t TCP

Note
----

The only purpose of this tool is to test the collector and its plugins on
previously captured flow packets. Performance of the tool is not ideal and it's
not intended to be used in production environment.

