Forwarder (output plugin)
=========================

This plugin allows forwarding incoming flow records in IPFIX form to other collector in various modes.

It can be used to broadcast messages to multiple collectors (e.g. a main and a backup collector),
or to distribute messages across multiple collectors (e.g. for load balancing).

Example configuration
---------------------

.. code-block:: xml

    <output>
        <name>Forwarder</name>
        <plugin>forwarder</plugin>
        <params>
            <mode>roundrobin</mode>
            <protocol>tcp</protocol>
            <hosts>
                <host>
                    <name>Subcollector 1</name>
                    <address>192.168.1.2</address>
                    <port>4739</port>
                </host>
                <host>
                    <name>Subcollector 2</name>
                    <address>192.168.1.3</address>
                    <port>4739</port>
                </host>
                <host>
                    <name>Subcollector 3</name>
                    <address>localhost</address>
                    <port>4739</port>
                </host>
            </hosts>
        </params>
    </output>

Parameters
----------

:``mode``:
    Flow distribution mode. **RoundRobin** (each record will be delivered to one of hosts) or **All** (each record will be delivered to all hosts).
    [values: RoundRobin/All]

:``protocol``:
    The transport protocol to use.
    [values: TCP/UDP]

:``templatesResendSecs``:
    Send templates again every N seconds (UDP only).
    [value: number of seconds, default: 600, 0 = never]

:``templatesResendPkts``:
    Send templates again every N packets (UDP only).
    [value: number of packets, default: 5000, 0 = never]

:``reconnectSecs``:
    Attempt to reconnect every N seconds in case the connection drops (TCP only).
    [value: number of seconds, default: 10, 0 = don't wait]

:``premadeConnections``:
    Keep N connections open with each host so there is no delay in connecting once a connection is needed.
    [value: number of connections, default: 5]

:``hosts``:
    The receiving hosts.

    :``host``:
        :``name``:
            Optional identification of the host.
            [value: string, default: <address>:<port>]

        :``address``:
            The address of the host.
            [value: IPv4/IPv6 address or a hostname]

        :``port``:
            The port to connect to.
            [value: port number]

Known limitations
-----------------

Export time of IPFIX messages is set to the current time when forwarding. This may cause issues with data fields using deltaTime relative to the export time!


