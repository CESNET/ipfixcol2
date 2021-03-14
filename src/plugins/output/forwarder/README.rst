Forwarder (output plugin) [Preview]
===================================

This plugin allows forwarding incoming IPFIX messages to other collector in various modes.

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
                    <address>127.0.0.1</address>
                    <port>4751</port>
                </host>
                <host>
                    <name>Subcollector 2</name>
                    <address>localhost</address>
                    <port>4752</port>
                </host>
            </hosts>
        </params>
    </output>

Parameters
----------

:``mode``:
    The forwarding mode; round robin (messages are sent to one host at time and hosts are cycled through) or all (messages are broadcasted to all hosts)
    [values: RoundRobin/All]

:``protocol``:
    The transport protocol to use
    [values: TCP/UDP]

:``connectionBufferSize``:
    Size of the buffer of each connection (Warning: number of connections = number of input exporters * number of hosts)
    [value: number of bytes, default: 4194304]

:``templateRefreshIntervalSecs``:
    Send templates again every N seconds (UDP only)
    [value: number of seconds, default: 600]

:``templateRefreshIntervalBytes``:
    Send templates again every N bytes (UDP only)
    [value: number of bytes, default: 5000000]

:``reconnectIntervalSecs``:
    Attempt to reconnect every N seconds in case the connection drops (TCP only)
    [value: number of seconds, default: 10]

:``hosts``:
    The receiving hosts

    :``host``:
        :``name``:
            Optional identification of the host
            [value: string, default: <address>:<port>]

        :``address``:
            The address of the host
            [value: IPv4/IPv6 address or a hostname]

        :``port``:
            The port to connect to
            [value: port number]

