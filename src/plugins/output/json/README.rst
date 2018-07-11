JSON (output plugin)
====================

The plugin converts IPFIX flow records into JSON format and sends them to remote destinations over
network, stores them into files or prints them on standard output. Some elements (timestamps,
IP/MAC addresses, TCP flags, etc.) can be converted into human readable format.
Others are represented as plain numbers.

Each flow record is converted individually and send to outputs where at least one output
method must be configured. All fields in an IPFIX record are formatted as: <field name>:<value>,
where the *field name* represents a name of a corresponding Information Element as
"<Enterprise Name>:<Information Element Name>" and *value* is a number, string,
boolean or null (if conversion from IPFIX to JSON fails). The enterprise name is a name of an
organization that manages the field, such as IANA, CISCO, etc.

If the field name and type is unknown and "ignoreUnknown" option is disabled, the field name
use format 'enXX:idYY' where XX is an Enterprise Number and YY is an Information Element ID.
See notes for instructions on how to add missing definitions of Information Elements.

Example output
--------------

The following output is for illustration purpose only. Available IPFIX fields depend on
a structure of records that your exporter sends. See documentation of the exporter,
if you want to change available IPFIX fields.

.. code-block:: json

    {
        "@type": "ipfix.entry",
        "iana:octetDeltaCount": 3568720,
        "iana:packetDeltaCount": 2383,
        "iana:flowStartMilliseconds": "2018-05-11T19:44:29.006Z",
        "iana:flowEndMilliseconds": "2018-05-11T19:44:30.364Z",
        "iana:ingressInterface": 1,
        "iana:ipVersion": 4,
        "iana:sourceIPv4Address": "192.168.0.100",
        "iana:destinationIPv4Address": "192.168.0.101",
        "iana:ipClassOfService": 0,
        "iana:ipTTL": 62,
        "iana:protocolIdentifier": "TCP",
        "iana:tcpControlBits": ".AP.S.",
        "iana:sourceTransportPort": 47200,
        "iana:destinationTransportPort": 20810,
        "iana:egressInterface": 0,
        "iana:samplingInterval": 0,
        "iana:samplingAlgorithm": 0
    }

Example configuration
---------------------

Below you see a complex configuration with all available output options enabled.
Don't forget to remove (or comment) outputs that you don't want to use!

.. code-block:: xml

    <output>
        <name>JSON output</name>
        <plugin>json</plugin>
        <params>
            <tcpFlags>formatted</tcpFlags>
            <timestamp>formatted</timestamp>
            <protocol>formatted</protocol>
            <ignoreUnknown>true</ignoreUnknown>
            <ignoreOptions>true</ignoreOptions>
            <nonPrintableChar>true</nonPrintableChar>

            <outputs>
                <server>
                    <name>Local server</name>
                    <port>8000</port>
                    <blocking>no</blocking>
                </server>

                <send>
                    <name>Send to my server</name>
                    <ip>127.0.0.1</ip>
                    <port>8000</port>
                    <protocol>tcp</protocol>
                    <blocking>no</blocking>
                </send>

                <print>
                    <name>Printer to standard output</name>
                </print>

                <file>
                    <name>Store to files</name>
                    <path>/tmp/ipfixcol/flow/%Y/%m/%d/</path>
                    <prefix>json.</prefix>
                    <timeWindow>300</timeWindow>
                    <timeAlignment>yes</timeAlignment>
                </file>
            </outputs>
        </params>
    </output>

Parameters
----------

Formatting parameters:

:``tcpFlags``:
    Convert TCP flags to common textual representation (formatted, e.g. ".A..S.")
    or to a number (raw). [values: formatted/raw, default: formatted]

:``timestamp``:
    Convert timestamp to ISO 8601 textual representation (all timestamps in UTC and milliseconds,
    e.g. "2018-01-22T09:29:57.828Z") or to a unix timestamp (all timestamps in milliseconds).
    [values: formatted/unix, default: formatted]

:``protocol``:
    Convert protocol identification to formatted style (e.g. instead 6 writes "TCP") or to a number.
    [values: formatted/raw, default: formatted]

:``ignoreUnknown``:
    Skip unknown Information Elements (i.e. record fields with unknown name and data type).
    If disabled, data of unknown elements are formatted as unsigned integer (the size of the
    field ≤ 8 bytes) or binary values. [values: true/false, default: true]

:``ignoreOptions``:
    Skip non-flow records used for reporting metadata about IPFIX Exporting and Metering Processes
    (i.e. records described by Options Templates). [values: true/false, default: true]

:``nonPrintableChar``:
    Ignore non-printable characters (newline, tab, control characters, etc.) in IPFIX strings.
    If disabled, these characters are escaped on output. [values: true/false, default: true]

----

Output types: At least one of the following output must be configured. Multiple server/send/file
outputs can be used at the same time if the outputs are not in collision with each other.

:``server``:
    TCP (push) server provides data on a local port. Converted records are automatically send to
    all clients that are connected to the port. To test the server you can use, for example,
    ``ncat(1)`` utility: "``ncat <server ip> <port>``".

    :``name``: Identification name of the output. Used only for readability.
    :``port``: Local port number of the server.
    :``blocking``:
        Enable blocking on TCP sockets (true/false). If blocking mode is disabled and
        a client is not able to retrieve records fast enough, some flow records may be dropped
        (only individual clients are affected). On the other hand, if the blocking mode is enabled,
        no records are dropped. However, if at least one client is slow, the plugin waits (i.e.
        blocks) until data are send. This can significantly slow down the whole collector and other
        output plugins because processing records is suspended. In the worst-case scenario,
        if the client is not responding at all, the whole collector is blocked! Therefore,
        it is usually preferred (and much safer) to disable blocking.

:``send``:
    Send records over network to a client. If the destination is not reachable or the client
    is disconnected, the plugin drops all records and tries to reconnect every 5 seconds.
    As with the server, you can verify functionality using ``ncat(1)`` utility:
    "``ncat -lk <local ip> <local port>``"

    :``name``: Identification name of the output. Used only for readability.
    :``ip``: IPv4/IPv6 address of the client
    :``port``: Remote port number
    :``protocol``: Transport protocol: TCP or UDP (this field is case insensitive)
    :``blocking``:
        Enable blocking on a socket (true/false). See the description of this property at the
        server output.

:``file``:
    Store data to files.

    :``name``: Identification name of the output. Used only for readability.
    :``path``:
        The path specifies storage directory for data collected by the plugin. Format specifiers
        for day, month, etc. are supported so you can create suitable directory hierarchy.
        See "strftime" for conversion specification. (Note: UTC time)
    :``prefix``: Specifies name prefix for output files.
    :``timeWindow``:
        Specifies the time interval in seconds to rotate files [minimum 60, default 300]
    :``timeAlignment``:
         Align file rotation with next N minute interval (yes/no).

:``print``:
    Write data on standard output.

    :``name``: Identification name of the output. Used only for readability.

Notes
-----

If one or more Information Element definitions are missing, you can easily add them.
All definitions are provided by libfds library. See the project website for help.

For higher performance, it is advisable to use non-formatted conversion of IPFIX data types.
If performance matters, you should prefer, for example, timestamps as numbers over ISO 8601 strings.

Structured data types (basicList, subTemplateList, etc.) are not supported yet.
