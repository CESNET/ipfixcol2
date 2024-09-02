Flow Data Storage (output plugin) [beta version]
================================================

The plugin converts and stores IPFIX Data Records into FDS file format. The file
is based on IPFIX, therefore, it provides highly-effective way for long-term
storage and stores complete flow records (including all Enterprise-specific
fields, biflow, etc.) together with identification of the flow exporters who
exported these records.

All data are stored into flat files, which are automatically rotated and renamed
every N minutes (by default 5 minutes).

    | **Warning**: The plugin is still under development and some incompatible
    | changes in file format may be introduced!

Example configuration
---------------------

.. code-block:: xml

    <output>
        <name>FDS output</name>
        <plugin>fds</plugin>
        <params>
            <storagePath>/tmp/ipfixcol2/fds/</storagePath>
            <compression>none</compression>
            <dumpInterval>
                <timeWindow>300</timeWindow>
                <align>yes</align>
            </dumpInterval>
            <outputSelection>
                <element>iana:octetDeltaCount</element>
                <element>iana@reverse:octetDeltaCount@reverse</element>
                <element>iana:packetDeltaCount</element>
                <element>iana@reverse:packetDeltaCount@reverse</element>
                <element>iana:destinationIPv4Address</element>
                <element>iana:destinationIPv6Address</element>
                <element>iana:sourceIPv4Address</element>
                <element>iana:sourceIPv6Address</element>
                <element>iana:destinationTransportPort</element>
                <element>iana:sourceTransportPort</element>
                <element>iana:protocolIdentifier</element>
                <element>cesnet:quicSNI</element>
                <element>cesnet:TLSSNI</element>
            </outputSelection>
        </params>
    </output>

Parameters
----------

:``storagePath``:
    The path element specifies the storage directory for data files. Keep on
    mind that the path must exist in your system. Otherwise, no files are stored.
    All files will be stored based on the configuration using the following
    template: ``<storagePath>/YYYY/MM/DD/flows.<ts>.fds`` where ``YYYY/MM/DD``
    means year/month/day and ``<ts>`` represents a UTC timestamp in
    format ``YYMMDDhhmmss``.

:``compression``:
    Data compression helps to significantly reduce size of output files.
    Following compression algorithms are available:

    :``none``: Compression disabled [default]
    :``lz4``:  LZ4 compression (very fast, slightly worse compression ration)
    :``zstd``: ZSTD compression (slightly slower, good compression ration)

:``dumpInterval``:
    Configuration of output files rotation.

    :``timeWindow``:
        Specifies time interval in seconds to rotate files i.e. close the current
        file and create a new one. [default: 300]

    :``align``:
        Align file rotation with next N minute interval. For example, if enabled
        and window size is 5 minutes long, files will be created at 0, 5, 10, etc.
        [values: yes/no, default: yes]

:``outputSelection``:
    Select only a subset of fields that will be saved in output. A list of IPFIX
    elements is provided. Any fields of an element that are not a part of this list
    will be skipped and not included in the resulting FDS output file.
    Note that when
    [default: all fields]

    :``element``:
        The identifier of the IPFIX element in the form of ``SCOPE_NAME:ELEMENT_NAME``,
        for example ``iana:sourceIPv4Address``. ``SCOPE_NAME:`` prefix can be omitted in
        case of "iana:".
        Note: Reverse elements use the following naming pattern
        ``iana@reverse:sourceIPv4Address@reverse``

:``asyncIO``:
    Allows to use asynchronous I/O for writing to the file. Usually when parts
    of the file are being written, the process is blocked on synchronous I/O
    and waits for the operation to complete. However, asynchronous I/O allows
    the plugin to simultaneously write to file and process flow records, which
    significantly improves overall performance. (Note: a pool of service
    threads shared among instances of FDS plugin might be created).
    [values: true/false, default: true]
