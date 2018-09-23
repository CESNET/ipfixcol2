UniRec (output plugin)
======================

The plugin converts IPFIX flow records into UniRec format and sends them as UniRec records via
a TRAP output interface. Structure of the output record is defined using a UniRec template that
consists of one or more UniRec fields.

Each flow record is converted individually. For each IPFIX field the plugin tries to find mapping
to a particular UniRec field of the output record and performs data conversion. It is possible
that the original IPFIX record doesn't contain all required fields. In this case, a user could
choose to drop the whole record or fill undefined UniRec fields with default values.
Converted records are sent over a unix socket/tcp/tcp-tls for further processing or stored as
files.

An instance of the plugin can use only one TRAP output interface and one UniRec template at the
same time. However, it is possible to create multiple instances of this plugin with different
configuration. For example, one instance can convert all flows using a UniRec template that
consists of common fields available in all flow records and another instance converts only
flow records that consist of HTTP fields.

How to build
------------

By default, the plugin is not distributed with IPFIXcol due to extra dependencies.
To build the plugin, IPFIXcol (and its header files) and the following dependencies must be
installed on your system:

- `Nemea Framework <https://github.com/CESNET/Nemea-Framework/tree/master>`_
  (or `Nemea <https://github.com/CESNET/Nemea>`_ project)

Finally, compile and install the plugin:

.. code-block:: sh

    $ mkdir build && cd build && cmake ..
    $ make
    # make install

Example configuration
---------------------

The following output instance configuration describes all supported types of TRAP output
interfaces, however, only one can be used. Therefore, remove all interfaces in ``<trapIfcSpec>``
except the one you would like to use.

.. code-block:: xml

    <output>
        <name>UniRec plugin</name>
        <plugin>unirec</plugin>
        <params>
            <uniRecFormat>TIME_FIRST,TIME_LAST,SRC_IP,DST_IP,PROTOCOL,?SRC_PORT,?DST_PORT,?TCP_FLAGS,PACKETS,BYTES</uniRecFormat>
            <trapIfcCommon>
                <timeout>NO_WAIT</timeout>
                <buffer>true</buffer>
                <autoflush>500000</autoflush>
            </trapIfcCommon>

            <trapIfcSpec>
                <!-- Only ONE of the following output interface! -->
                <tcp>
                    <port>8000</port>
                    <maxClients>64</maxClients>
                </tcp>
                <tcp-tls>
                    <port>8000</port>
                    <maxClients>64</maxClients>
                    <keyFile>...</keyFile>
                    <certFile>...</certFile>
                    <caFile>...</caFile>
                </tcp-tls>
                <unix>
                    <name>ipfixcol-output</name>
                    <maxClients>64</maxClients>
                </unix>
                <file>
                    <name>/tmp/nemea/trapdata</name>
                    <mode>write</mode>
                </file>
            </trapIfcSpec>
        </params>
    </output>

Parameters
----------

:``uniRecFormat``:
    Comma separated list of UniRec fields. All fields are mandatory be default, therefore, if
    a flow record to convert doesn't contain all mandatory fields, it is dropped.
    However, UniRec fields that start with '?' are optional and if they are not present in the
    record (e.g. TCP_FLAGS) default value (typically zero) is used. List of all supported UniRec
    fields is defined in `unirec-element.txt <config/unirec-elements.txt>`_ file.
    Example value: "DST_IP,SRC_IP,BYTES,DST_PORT,?TCP_FLAGS,SRC_PORT,PROTOCOL".

:``trapIfcCommon``:
    The following parameters can be used with any type of a TRAP interface. There are parameters
    of the interface that are normally let default. However, it is possible to override them
    by user:

    :``timeout``:
        Time in microseconds that the output interface can block waiting for message to be send.
        There are also special values: "WAIT" (block indefinitely), "NO_WAIT" (don't block),
        "HALF_WAIT" (block only if some client is connected). Be very careful, inappropriate
        configuration can significantly slowdown the collector and lead to loss of data.
        [default: "HALF_WAIT"]

    :``buffer``:
        Enable buffering of data and sending in larger bulks (increases throughput)
        [default: true]

    :``autoflush``:
        Automatically flush data even if the output buffer is not full every X microseconds.
        If the automatic flush is disabled (value 0), data are not send until the buffer is full.
        [default: 500000]

:``trapIfcSpec``:
    Specification of interface type and its parameters. For more details, see section
    "Output interface types".

Output interface types
----------------------
Exactly one of the following output type must be defined in the instance configuration of this
plugin.

:``unix``:
    Communicates through a UNIX socket. The output interface creates a socket and listens, input
    interface connects to it. There may be more than one input interfaces connected to the output
    interface, every input interface will get the same data. Parameters:

    :``name``:
        Socket name i.e. any string usable as a file name. The name MUST not include colon
        character.

    :``maxClients``:
        Maximal number of connected clients (input interfaces). [default: 64]

:``tcp``:
    Communicates through a TCP socket. The output interface listens on a given port, input
    interface connects to it. There may be more than one input interfaces connected to the output
    interface, every input interface will get the same data. Parameters:

    :``port``:
        Local port number

    :``maxClients``:
        Maximal number of connected clients (input interfaces). [default: 64]

:``tcp-tls``:
    Communicates through a TCP socket after establishing encrypted connection. You have to
    provide a certificate, a private key and a CA chain file with trusted CAs. Otherwise, same
    as TCP: The output interface listens on a given port, input interface connects to it.
    There may be more than one input interfaces connected to the output interface,
    every input interface will get the same data. Paths to files MUST not include colon character.
    Parameters:

    :``port``:
        Local port number

    :``maxClients``:
        Maximal number of connected clients (input interfaces). [default: 64]

    :``keyFile``:
        Path to a file of a private key in PEM format.

    :``certFile``:
        Path to a file of certificate chain in PEM format.

    :``caFile``:
        Path to a file of trusted CA certificates in PEM format.

:``file``:
    Stores UniRec records into a file. The interface allows to split data into multiple files
    after a specified time or a size of the file. If both options are enabled at the same time,
    the data are split primarily by time, and only if a file of one time interval exceeds
    the size limit, it is further split. The index of size-split file is appended after the
    time. Parameters:

    :``name``:
        Name of the output file. The name MUST not include colon character.

    :``mode``:
        Output mode: ``write``/``append``. If the specified file exists, mode ``write`` overwrites
        it, mode append creates a new file with an integer suffix. [default: ``write``]

    :``time``:
        If the parameter is non-zero, the output interface will split captured data to individual
        files as often, as value of this parameter (in minutes) indicates. The output interface
        creates unique file name for each file according to current timestamp in format:
        "filename.YYYYmmddHHMM". [default: 0]

    :``size``:
        If the parameter is non-zero, the output interface will split captured data into individual
        files after a size of a current file (in MB) exceeds given threshold. Numeric suffix is
        added to the original file name for each file in ascending order starting with 0.
        [default: 0]


UniRec configuration file
-------------------------

Conversion from IPFIX fields to UniRec fields is defined in the configuration file
`unirec-element.txt <config/unirec-elements.txt>`_. The file is distributed and installed together
with the plugin and it is usually placed in the same directory as the default IPFIXcol startup
configuration (see ``ipfixcol2 -h`` for help).

The structure of the file is simple. Every line corresponds to one UniRec field and consists of
three mandatory parameters (name, type, list of IPFIX Information Elements). For example,
a line: ``"BYTES uint64 e0id1"``:

- First parameter specifies an UniRec name. This name is used in a plugin configuration in
  the ``<uniRecFormat>`` element.
- Second parameter specifies a data type of the UniRec field. List of all supported types is available
  in `UniRec documentation <https://github.com/CESNET/Nemea-Framework/tree/master/unirec>`_.
- The third parameter is comma separated list of corresponding IPFIX Information Elements (IEs). In
  this case, "e0id1" means IPFIX IE with Private Enterprise ID 0 and IE ID 1 (which is
  "octetDeltaCount"). Instead of numeric identification an IE name in "<scope>:<name>" format
  can be also used, for example, ``"BYTES uint64 iana:octetDeltaCount"``.

How to add a new conversion record
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

First of all, make sure that definitions of IPFIX Information Elements to convert are known to
the IPFIXcol. The plugin needs to know the definitions to find appropriate conversion functions
from IPFIX to UniRec data types. All definitions of IEs are provided
by `libfds <https://github.com/CESNET/libfds/>`_ library and if any definition is missing, it is
possible to add it manually to configuration. See the project website for help.

Now you can create a new entry in the configuration file. All three parameters, i.e. UniRec field
name, UniRec type and list of IPFIX IEs, must be defined. If the mapping is configured correctly
the new UniRec field can be used in the template specification i.e. ``<uniRecFormat>``.

Be aware that data types of IPFIX IEs and corresponding UniRec fields could be slightly different.
When a value of an IPFIX field cannot fit into a UniRec field (e.g. conversion from 64b
to 32b unsigned integer), the converted value is saturated (the maximum/minimum possible
value is used) or extra bits are discarded. To distinguish these situation, the data semantic
of used IPFIX IE is used. If its semantic is ``flags`` or ``identifier``, extra bits are
discarded. Otherwise, the value is saturated.

  Note: If saturation is applied and a negative integer is converted from signed to unsigned
  integer type, the result value is zero.

To see conversion warnings, add the UniRec field to ``<uniRecFormat>`` and run the collector
with increased verbosity level i.e. ``ipfixcol2 -v``.

Notes
-----

Bidirectional flows are not currently supported by UniRec, therefore, biflow records are
automatically split into two unidirectional flow records during conversion.

When multiple IPFIX Information Elements are mapped to the same UniRec field and those IPFIX fields
are present in an IPFIX record, the last field occurrence (in the appropriate IPFIX Template)
is converted to the UniRec field.

TODO: describe "link_bit_field" + "dir_bit_field"

