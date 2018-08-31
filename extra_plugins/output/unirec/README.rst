UniRec (output plugin)
======================

The plugin converts flow records into UniRec records and sends that via TRAP output interface.


Example configuration
---------------------

Below you can see a configuration with....

Parameters
----------

:``uniRecFormat``:
    Comma separated list of UniRec fields. All fields are mandatory be default, therefore, if
    a flow record to convert doesn't contain all mandatory fields, it will be dropped.
    UniRec fields that start with '?' are optional and they are filled with default values if
    not present in the record (e.g. TCP_FLAGS).
    For example, "DST_IP,SRC_IP,BYTES,DST_PORT,?TCP_FLAGS,SRC_PORT,PROTOCOL".
    List of all supported UniRec fields is defined in `unirec-element.txt <unirec-elements.txt>`_
    file.

:``trapIfcCommon``:
    The following parameters can be used with any type of a TRAP interface. There are parameters
    of the interface that are normally let default. It is possible to override them by user. The
    available parameters are:

    :``timeout``:
        Time in microseconds that the output interface can block waiting for message to be send.
        There are also special values: "WAIT" (block indefinitely), "NO_WAIT" (don't block),
        "HALF_WAIT" (block only if some client is connected). [default: "HALF_WAIT"]

    :``buffer``:
        Enable buffering of data and sending in larger bulks (increases throughput)
        [default: true]

    :``autoflush``:
        Automatically flush data even if the output buffer is not full every X microseconds.
        If the automatic flush is disabled (value 0), data are not send until the buffer is full.
        [default: 500000]

:``trapIfcSpec``:
    Specification of interface type and its parameters. Only one of the following output type can
    be used at the same time: unix, tcp, tcp-tls, file. For more details, see section
    "Output interface types".

Output interface types
----------------------

:``unix``:
    Communicates through a UNIX socket. The output interface creates a socket and listens, input
    interface connects to it. There may be more than one input interfaces connected to the output
    interface, every input interface will get the same data. Parameters:

    :``name``:
        Socket name. Any string usable as a file name.

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
    provide a certificate, a private key and a CA chain file with trusted CAs. Otherwise same
    as TCP: The output interface listens on a given port, input interface connects to it.
    There may be more than one input interfaces connected to the output interface,
    every input interface will get the same data. Parameters:

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
    Store UniRec records into a file. The interface allows to split data into multiple files
    after a specified time or a size of the file. If both options are enabled at the same time,
    the data are split primarily by time, and only if a file of one time interval exceeds
    the size limit, it is further splitted. The index of size-splitted file is appended after the
    time. Parameters:

    :``name``:
        Name of the output file.

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
