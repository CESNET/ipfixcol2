lnfstore (output plugin)
========================

The plugin converts and store IPFIX flow records into NfDump compatible files. Only a subset of
IPFIX fields that have NetFlow equivalents are stored into NfDump files. Other fields are discarded.
Biflow records are split into two unidirectional flow records.

To speed up search of flow records of an IP address in multiple data files, the plugin can also
create index files. These files will be created simultaneously with data files and they can be
utilized by tools such as *fdistdump* to promptly determine if there is at least one record
with the specified IP address in a file. This can dramatically reduce the number of processed
files and provide query results faster.

How to build
------------

By default, the plugin is not distributed with IPFIXcol due to extra dependencies.
To build the plugin, IPFIXcol (and its header files) and the following dependencies must be
installed on your system:

- `libnf <https://github.com/VUTBR/libnf>`_
- `bloom-filter-indexes <https://github.com/CESNET/bloom-filter-index/>`_

Finally, compile and install the plugin:

.. code-block:: sh

    $ mkdir build && cd build && cmake ..
    $ make
    # make install

Example configuration
---------------------

.. code-block:: xml

    <output>
        <name>LNF storage</name>
        <plugin>lnfstore</plugin>
        <params>
            <storagePath>/tmp/ipfixcol/</storagePath>
            <compress>yes</compress>
            <dumpInterval>
                <timeWindow>300</timeWindow>
                <align>yes</align>
            </dumpInterval>
            <index>
                <enable>yes</enable>
                <autosize>yes</autosize>
            </index>
        </params>
    </output>

**Warning**:  The storage path *must* already exist in your system. Otherwise all data will be lost.

Parameters
----------

:``storagePath``:
    The path element specifies the storage directory for data files. Keep on mind that the path
    must exist in your system. Otherwise, no records are stored. All files will be stored based
    on the configuration using the following template:
    ``<storagePath>/YYYY/MM/DD/lnf.<suffix>`` where ``YYYY/MM/DD`` means year/month/day and
    ``<suffix>`` represents a UTC timestamp in format ``YYMMDDhhmmss``.

:``compress``:
    Enable/disable LZO compression for files. [values: yes/no, default: no]

:``identificatorField``:
    Specifies an identification string, which is put into statistic records to describe
    the source. [default: <empty>]

:``dumpInterval``:
    Configuration of output files rotation.

    :``timeWindow``:
        Specifies time interval in seconds to rotate files i.e. close the current file and create
        a new one. [default: 300]

    :``align``:
        Align file rotation with next N minute interval. For example, if enabled and window
        size is 5 minutes long, files will be created at 0, 5, 10, etc.
        [values: yes/no, default: yes]

:``index``:
    Configuration of IP address indexes. Index files are independent and exists besides
    "lnf.*" files as "bfi.*" files with matching identification.

    :``enable``:
        Enable/disable Bloom Filter indexes. [values: yes/no, default: no]

    :``autosize``:
        Enable/disable automatic resize of index files based on the number of unique IP addresses
        in the last dump interval. [values: yes/no, default: yes]

    :``estimatedItemCount``:
        Expected number of unique IP addresses in dump interval. If autosize is enabled this
        value is continuously recalculated to suit current utilization. The value affects the
        size of index files i.e. higher value, larger files. [default: 100000]

    :``falsePositiveProbability``:
        False positive probability of the index. The probability that presence test of an IP
        address indicates that the IP address is present in a data file, when it actually is not.
        It does not affect the situation when the IP address is actually in the data file i.e.
        if the IP is in the file, the result of the test is always correct. The value affects
        the size of index files i.e. smaller value, larger files. [default: 0.01]
