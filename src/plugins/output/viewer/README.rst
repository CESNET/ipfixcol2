Viewer (output plugin)
=====================

The plugin converts IPFIX Messages into plain text and prints them on standard output.

The main goal of the plugin is to show content of received IPFIX Messages in human readable form.
Each IPFIX Message is broken down into IPFIX Sets and each IPFIX Set is further broken down into
(Options) Template/Data records and so on. Fields of the Data records are formatted according
to the expected data type, if their corresponding Information Element definitions are known to
the collector. Therefore, the output can be also used to determine missing Information Element
definitions.

Biflow records and structured data types are also supported and appropriately formatted.
Output is not supposed to be further parsed because its format can be changed in the future.
However, if you are interested into processing Data Records, consider using other
plugins such as JSON, UniRec, etc.

Example configuration
---------------------

.. code-block:: xml

    <output>
        <name>Viewer output</name>
        <plugin>viewer</plugin>
        <params/>
    </output>

Parameters
----------

Parameters are not supported by the plugin.

Example output
--------------

Below you can see example output for anonymized IPFIX Messages. The message structure and
available fields of Data Record vary from exporter to exporter.

.. code-block::

    IPFIX Message header:
        Version:      10
        Length:       176
        Export time:  1455534348
        Sequence no.: 0
        ODID:         1

    Set Header:
        Set ID: 2 (Template Set)
        Length: 160
    Template Record (#1)
        Template ID: 256
        Field Count: 27
        EN: 0  ID: 1    Size: 4  |  iana:octetDeltaCount
        EN: 0  ID: 2    Size: 4  |  iana:packetDeltaCount
        EN: 0  ID: 152  Size: 8  |  iana:flowStartMilliseconds
        EN: 0  ID: 153  Size: 8  |  iana:flowEndMilliseconds
        EN: 0  ID: 10   Size: 2  |  iana:ingressInterface
        EN: 0  ID: 60   Size: 1  |  iana:ipVersion
        EN: 0  ID: 8    Size: 4  |  iana:sourceIPv4Address
        EN: 0  ID: 12   Size: 4  |  iana:destinationIPv4Address
        EN: 0  ID: 5    Size: 1  |  iana:ipClassOfService
        EN: 0  ID: 192  Size: 1  |  iana:ipTTL
        EN: 0  ID: 4    Size: 1  |  iana:protocolIdentifier
        EN: 0  ID: 7    Size: 2  |  iana:sourceTransportPort
        EN: 0  ID: 11   Size: 2  |  iana:destinationTransportPort
        ... <~ output shortened for clarity ~> ...

    -------------------------------------------------------------------
    IPFIX Message header:
        Version:      10
        Length:       216
        Export time:  1455534348
        Sequence no.: 0
        ODID:         1

    Set Header:
        Set ID: 256 (Data Set)
        Length: 200
        Template ID: 256
    Data Record (#1) [Length: 95]:
        EN: 0  ID: 1    iana:octetDeltaCount          : 76 octets
        EN: 0  ID: 2    iana:packetDeltaCount         : 1 packets
        EN: 0  ID: 152  iana:flowStartMilliseconds    : 2016-02-15T11:05:48.150Z
        EN: 0  ID: 153  iana:flowEndMilliseconds      : 2016-02-15T11:05:48.150Z
        EN: 0  ID: 10   iana:ingressInterface         : 1
        EN: 0  ID: 60   iana:ipVersion                : 4
        EN: 0  ID: 8    iana:sourceIPv4Address        : 209.246.218.165
        EN: 0  ID: 12   iana:destinationIPv4Address   : 93.113.168.59
        EN: 0  ID: 5    iana:ipClassOfService         : 0
        EN: 0  ID: 192  iana:ipTTL                    : 122 hops
        EN: 0  ID: 4    iana:protocolIdentifier       : 17
        EN: 0  ID: 7    iana:sourceTransportPort      : 62299
        EN: 0  ID: 11   iana:destinationTransportPort : 53
        ... <~ output shortened for clarity ~> ...
