UDP (input plugin)
==================

The plugin receives IPFIX messages over UDP transport protocol from one or more exporters
and pass them into the collector. Multiple instances of the plugin can run concurrently.
However, they must listen on different ports or local IP addresses.

Keep in mind that connection between the exporter and the collector is unreliable because of UDP and
IPFIX protocol is unidirectional (the collector can only receive). Because various exporters
can measure different features of flows, IPFIX specifies templates that describe structure
of transmitted flow records and the collector uses these templates to interpret them. In case of
connection over UDP, templates are regularly retransmitted by exporters. However, if the collector
starts after an exporter, it could miss all templates and it is unable to interpret
records until next retransmission. *In other words, it may take up to a few minutes for the
first record to be successfully interpret* (depends on template retransmission interval of the
exporter).

**Warning**: One of the most common causes of lost flow records transmitted over networks
using UDP protocol is an undersized receive buffer of a system socket. The actual size of the buffer
your operating system provides is limited by operating system-level maximums. The plugin
automatically changes default size to the maximum size during startup. However, the size is usually
not optimal for monitoring high-speed network or reception from multiple exporters.
It is recommended to increase the limits before the start of the collector. For the most Linux
distributions, you can use, for example, following command (value in bytes):

.. code-block:: bash

    sysctl -w net.core.rmem_max=16777216


Example configuration
---------------------

.. code-block:: xml

    <input>
        <name>UDP input</name>
        <plugin>udp</plugin>
        <params>
            <localPort>4739</localPort>
            <localIPAddress></localIPAddress>
            <!-- Optional parameters -->
            <connectionTimeout>600</connectionTimeout>
            <templateLifeTime>1800</templateLifeTime>
            <optionsTemplateLifeTime>1800</optionsTemplateLifeTime>
        </params>
    </input>

Parameters
----------

Mandatory parameters:

:``localPort``:
    Local port on which the plugin listens. [default: 4739]
:``localIPAddress``:
    Local IPv4/IPv6 address on which the UDP input plugin listens. If the element
    is left empty, the plugin binds to all available network interfaces. The element can occur
    multiple times (one IP address per occurrence) to manually select multiple interfaces.
    [default: empty]

Optional parameters:

:``connectionTimeout``:
    Exporter connection timeout in seconds. If no message is received from an exporter
    for a specified time, the connection is considered closed and all resources (associated
    with the exporter) are removed, such as flow templates, etc. [default: 600]
:``templateLifeTime``, ``optionsTemplateLifeTime``:
    (Options) Template lifetime in seconds for all UDP Transport Sessions terminating at
    this UDP socket. (Options) Templates that are not received again within the configured
    lifetime become invalid. The lifetime of Templates and Options Templates should be at
    least three times higher than the same values configured on the corresponding exporter.
    [default: 1800]
