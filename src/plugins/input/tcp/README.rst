TCP (input plugin)
==================

The plugin receives IPFIX messages over TCP transport protocol from one or more exporters
and pass them into the collector. Multiple instances of the plugin can run concurrently.
However, they must listen on different ports or local IP addresses.

Unlike UDP, TCP is reliable transport protocol that allows exporters to detect connection or
disconnection of the collector. Therefore, the issues with templates retransmission and
initial period of inability to interpret flow records does not apply here.

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
