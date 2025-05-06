clickhouse (output plugin)
===========================

The plugin converts and stores IPFIX flow records into a ClickHouse database.
It is designed for high-performance environments, ensuring efficient data
handling and storage.

Key Features
------------

- **High-Speed Export**: The conversion and export of data to the database are
  performed at the binary level (i.e., without conversion to text SQL
  commands). This enables export speeds of hundreds of thousands or even a
  million flow records per second (*performance depends on machine
  configuration*).

- **Customizable Data Mapping**: Users can configure the plugin to send any
  IPFIX/NetFlow items to the database by specifying the item name and mapping
  it to the corresponding database column.

- **High Availability (HA) Support**: The plugin supports sending data to
  multiple ClickHouse endpoints. In case of a failure at one endpoint, data is
  automatically redirected to the next available endpoint. This failover
  mechanism ensures reliability in HA deployments. Note: This is not a
  round-robin distribution but a failover strategy.

How to build
------------

By default, the plugin is not distributed with IPFIXcol due to extra dependencies.
To build the plugin, IPFIXcol2 (and its header files) must be installed on your system.

Finally, compile and install the plugin:

::

    $ mkdir build && cd build && cmake ..
    $ make
    # make install

Usage
------

The plugin expects the ClickHouse database to already contain the table with
appropriate schema corresponding to the configuration entered. The existence
and schema of the table is checked after initiating connection to the database
and an error is displayed if there is a mismatch. The table is not
automatically created.

To aid in initial setup, a script `schema-helper.py` is included. For more information,
see `Schema helper script <#schema-helper>`_.

To run the example configuration below, you can create the ClickHouse table
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

The following ClickHouse column types are expected to store the following IPFIX element types:

.. list-table:: Mapping of IPFIX types to ClickHouse column types

    * - **IPFIX Abstract Data Type**
      - **ClickHouse Column Type**
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
    * - macAddress
      - UInt64
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

To be able to retrieve the stored values in a human-friendly format, the
following ClickHouse functions can be defined and used:

.. code-block:: sql

    CREATE FUNCTION ipToString AS (ip) ->
        if(isIPAddressInRange(toString(ip), '::ffff:0.0.0.0/96'), toString(toIPv4(ip)), toString(ip));

    CREATE FUNCTION macToString AS (mac) ->
        concat(
            lpad(hex(bitAnd(bitShiftRight(mac, 40), 0xFF)), 2, '0'), ':',
            lpad(hex(bitAnd(bitShiftRight(mac, 32), 0xFF)), 2, '0'), ':',
            lpad(hex(bitAnd(bitShiftRight(mac, 24), 0xFF)), 2, '0'), ':',
            lpad(hex(bitAnd(bitShiftRight(mac, 16), 0xFF)), 2, '0'), ':',
            lpad(hex(bitAnd(bitShiftRight(mac, 8), 0xFF)), 2, '0'), ':',
            lpad(hex(bitAnd(mac, 0xFF)), 2, '0')
        );


Example configuration
---------------------

.. code-block:: xml

    <output>
        <name>ClickHouse output</name>
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
            <inserterThreads>8</inserterThreads>
            <blocks>64</blocks>
            <blockInsertThreshold>100000</blockInsertThreshold>
            <splitBiflow>true</splitBiflow>
            <nonblocking>true</nonblocking>
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
                The ClickHouse database host as a domain name or an IP address.

            :``port``:
                The port of the ClickHouse database. [default: 9000]

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
    buffer that the rows are written to before being sent out to the ClickHouse
    database. [default: 64]

:``inserterThreads``:
    Number of threads used for data insertion to ClickHouse. In other words,
    the number of ClickHouse connections that are concurrently used. [default: 8]

:``blockInsertThreshold``:
    Number of rows to be buffered into a block before the block is sent out to
    be inserted into the database. [default: 100000]

:``blockInsertMaxDelaySecs``:
    Maximum number of seconds to wait before a block gets sent out to be
    inserted into the database even if the threshold has not been reached yet.
    [default: 10]

:``nonblocking``:
    This option dictates what happens when all the blocks (buffers) are full.
    If true, the processing thread is not blocked, and some data is dropped to
    maintain flow of data.
    If false, the processing thread is blocked, waiting until a block becomes
    available. [default: true]

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
            Aliases and IPFIX elements can be found
            `here <https://github.com/CESNET/libfds/tree/master/config/system>`_.
            List of standard IPFIX element names can be also found
            `here <https://www.iana.org/assignments/ipfix/ipfix.xhtml>`_.
            [default: same as name]

Performance tuning
------------------

In case you are having performance issues with the default values, try
increasing `blockInsertThreshold`, `blocks` and `inserterThreads` configuration
parameters.

For example based on our testing, the following values should result in better
performance at the cost of higher memory usage:

.. code-block:: xml

    <inserterThreads>16</inserterThreads>
    <blocks>128</blocks>
    <blockInsertThreshold>500000</blockInsertThreshold>

You can further experiment with the values based on your input characteristics
and your machine specifications.

Schema helper
--------------

To aid in initial setup, a script `schema-helper.py` is included. The script
runs ipfixcol2 for a brief period of time to observe structure of the flow data
that is being received, and then generates a ClickHouse schema SQL and a
ipfixcol2 config XML. The generated files can be used as an easier starting
point as opposed to doing everything manually.

::

    usage: schema-helper.py [-h] [-a ADDRESS] [-p PORT] [-i INTERVAL] [-t {tcp,udp}] [-s SCHEMA_FILE] [-c CONFIG_FILE] [-o OVERWRITE]

    optional arguments:
      -h, --help            show this help message and exit
      -a ADDRESS, --address ADDRESS
                            the local IP address, i.e. interface address; empty = all interfaces (default: )
      -p PORT, --port PORT  the local port (default: 4739)
      -i INTERVAL, --interval INTERVAL
                            the collection interval in seconds (default: 60)
      -t {tcp,udp}, --type {tcp,udp}
                            the input protocol type (default: udp)
      -s SCHEMA_FILE, --schema-file SCHEMA_FILE
                            the output schema file (default: schema.sql)
      -c CONFIG_FILE, --config-file CONFIG_FILE
                            the output config file (default: config.xml)
      -o OVERWRITE, --overwrite OVERWRITE
                            overwrite the output files without asking if they already exist (default: False)

**Note:**
In case UDP is used, you might need to increase the interval up to several
minutes for the collector to gather and decode enough data. If you are using
UDP and are getting no results, try running the script with interval set to 300
or even 600.


**Example usage:**

::

        $ schema-helper.py -i 60 -p 4739 -t tcp

Collect data for 60 seconds, listen on port 4739 using TCP.
