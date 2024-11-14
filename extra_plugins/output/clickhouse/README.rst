clickhouse (output plugin)
===========================

The plugin converts and store IPFIX flow records into a Clickhouse database.
files and provide query results faster.

How to build
------------

By default, the plugin is not distributed with IPFIXcol due to extra dependencies.
To build the plugin, IPFIXcol2 (and its header files) must be installed on your system.

Finally, compile and install the plugin:

.. code-block:: sh

    $ mkdir build && cd build && cmake ..
    $ make
    # make install

Usage
------

The plugin expects the Clickhouse database to already contain the table with
appropriate schema corresponding to the configuration entered. The existence
and schema of the table is checked after initiating connection to the database
and an error is displayed if there is a mismatch. The table is not
automatically created.

To run the example configuration below, you can create the Clickhouse table
using the following SQL query:

.. code-block:: sql

    CREATE TABLE ipfixcol2.flows (
        odid UInt32,
        srcip IPv6,
        dstip IPv6,
        flowstart DateTime64(9),
        flowend DateTime64(9),
        sourceTransportPort UInt16,
        destinationTransportPort UInt16,
        protocolIdentifier UInt8,
        octetDeltaCount UInt64,
        packetDeltaCount UInt64,
        INDEX srcipindex srcip TYPE bloom_filter GRANULARITY 16,
        INDEX dstipindex dstip TYPE bloom_filter GRANULARITY 16
    )
    ENGINE = MergeTree
    PARTITION BY toStartOfInterval(flowstart, INTERVAL 1 HOUR)
    ORDER BY flowstart

The following Clickhouse column types are expected to store the following IPFIX element types:

.. list-table:: Mapping of IPFIX types to Clickhouse column types

    * - **IPFIX Abstract Data Type**
      - **Clickhouse Column Type**
    * - unsigned8
      - UInt8
    * - unsigned16
      - UInt16
    * - unsigned32
      - UInt32
    * - unsigned64
      - UInt64
    * - signed8
      - Int8
    * - signed16
      - Int16
    * - signed32
      - Int32
    * - signed64
      - Int64
    * - ipv4Address
      - IPv4
    * - ipv6Address
      - IPv6
    * - dateTimeNanoseconds
      - DateTime64(9)
    * - dateTimeMicroseconds
      - DateTime64(6)
    * - dateTimeMilliseconds
      - DateTime64(3)
    * - dateTimeSeconds
      - DateTime
    * - string
      - String

In case the field is an alias mapping to multiple IPFIX elements of compatible
types, the resulting type is unified to the type with higher precision, i.e.
the type that can hold both of the values without data loss. To unify IPv4 and
IPv6 addresses as one type, the IPv4 is stored as an IPv6 value as a IPv4
mapped IPv6 address.


Example configuration
---------------------

.. code-block:: xml

    <output>
        <name>Clickhouse output</name>
        <plugin>clickhouse</plugin>
        <params>
            <connection>
                <endpoints>
                    <!-- One or more ClickHouse databases (endpoints) -->
                    <endpoint>
                        <host>clickhouse.example.com</host>
                        <port>9000</port>
                    </endpoint>
                </endpoints>
                <user>ipfixcol2</user>
                <password>ipfixcol2</password>
                <database>ipfixcol2</database>
                <table>flows</table>
            </connection>
            <inserterThreads>32</inserterThreads>
            <blocks>1024</blocks>
            <blockInsertThreshold>100000</blockInsertThreshold>
            <splitBiflow>true</splitBiflow>
            <columns>
                <column>
                    <!-- Special field representing the ODID the flow originated from. -->
                    <name>odid</name>
                </column>
                <column>
                    <!-- IPFIX field(s) identified by an alias. Maps to sourceIPv4Address or sourceIPv6Address, whichever exists. -->
                    <name>srcip</name>
                </column>
                <column>
                    <name>dstip</name>
                </column>
                <column>
                    <name>flowstart</name>
                </column>
                <column>
                    <name>flowend</name>
                </column>
                <column>
                    <!-- IPFIX field identified by its IANA name stored to a column named "srcport" -->
                    <name>srcport</name>
                    <source>sourceTransportPort</source>
                </column>
                <column>
                    <name>dstport</name>
                    <source>destinationTransportPort</source>
                </column>
                <column>
                    <!-- IPFIX field identified by its IANA name stored to a column with the same name -->
                    <name>protocolIdentifier</name>
                </column>
                <column>
                    <name>octetDeltaCount</name>
                </column>
                <column>
                    <name>packetDeltaCount</name>
                </column>
            </columns>
        </params>
    </output>

**Warning**:  The database and the table with the appropriate schema must already exist.
It will not be created automatically.

Parameters
----------

:``connection``:
    The database connection parameters.

    :``endpoints``:
        The possible endpoints data can be sent to, i.e. all the replicas of a
        particular shard. In case one endpoint is unreachable, another one is used.

        :``endpoint``:
            Connection parameters of one endpoint.

            :``host``:
                The Clickhouse database host as a domain name or an IP address.

            :``port``:
                The port of the Clickhouse database. [default: 9000]

    :``username``:"
        The database username.

    :``password``:
        The database password.

    :``database``:
        The database name where the specified table is present.

    :``table``:
        The name of the table to insert the data into.

:``splitBiflow``:
    When true, biflow records are split into two uniflow records. [default: true]

:``biflowEmptyAutoignore``:
    When true and ``splitBiflow`` is active, the uniflow records resulting from
    the split are also checked for emptiness and are omitted if empty. A flow
    is considered empty when ``octetDeltaCount = 0`` or ``packetDeltaCount = 0``.
    This exists because some IPFIX probes may export uniflow records as biflow
    with the reverse direction always empty, resulting in a large amount of
    empty flow records.
    [default: true]

:``blocks``:
    Number of data blocks in circulation. Each block is de-facto a memory
    buffer that the rows are written to before being sent out to the Clickhouse
    database. [default: 1024]

:``inserterThreads``:
    Number of threads used for data insertion to Clickhouse. In other words,
    the number of Clickhouse connections that are concurrently used. [default: 32]

:``blockInsertThreshold``:
    Number of rows to be buffered into a block before the block is sent out to
    be inserted into the database. [default: 100000]

:``blockInsertMaxDelaySecs``:
    Maximum number of seconds to wait before a block gets sent out to be
    inserted into the database even if the threshold has not been reached yet.
    [default: 10]

:``columns``:
    The fields that each row will consist of.

    :``column``:

        :``name``:
            Name of the column in the database. Also the source field if source
            is not explicitly defined.

        :``nullable``:
            Whether null should be a special value. If false, zero value of the
            corresponding data type is used as null. Turning this option on
            might negatively affect performance. [default: false]

        :``source``:
            An IPFIX element name or an alias. If not present, name is used.
            [default: same as name]
