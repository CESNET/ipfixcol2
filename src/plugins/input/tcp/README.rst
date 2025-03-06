TCP (input plugin)
==================

The plugin receives IPFIX messages over TCP transport protocol from one or more exporters
and pass them into the collector. Multiple instances of the plugin can run concurrently.
However, they must listen on different ports or local IP addresses.

Unlike UDP, TCP is reliable transport protocol that allows exporters to detect connection or
disconnection of the collector. Therefore, the issues with templates retransmission and
initial period of inability to interpret flow records does not apply here.

Received IPFIX messages may be compressed using LZ4 stream compression. The compression is
enabled on the exporter and the plugin detects it automatically.

Received IPFIX messages may be secured with TLS. The TLS decoder is enabled by specifying the
certificateFile parameter. Inbound TLS messages are automatically detected.

Example configuration
---------------------

.. code-block:: xml

    <input>
        <name>TCP input</name>
        <plugin>tcp</plugin>
        <params>
            <localPort>4739</localPort>
            <localIPAddress></localIPAddress>
        </params>
    </input>

Parameters
----------

Mandatory parameters:

:``localPort``:
    Local port on which the plugin listens. [default: 4739]
:``localIPAddress``:
    Local IPv4/IPv6 address on which the TCP input plugin listens. If the element
    is left empty, the plugin binds to all available network interfaces. The element can occur
    multiple times (one IP address per occurrence) to manually select multiple interfaces.
    [default: empty]

Optional parameters:

:``certificateFile``:
    Absolute path to a PEM file with certificate and private key. Specifying this path will enable
    TLS decoder for the inbound connections. If this parameter is omitted, TLS decoder is disabled.

Notes
-----
The LZ4 compression uses special format that compatible with
`ipfixproble <https://github.com/CESNET/ipfixprobe>`.
