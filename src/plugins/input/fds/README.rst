FDS File (input plugin)
=========================

The plugin reads flow data from one or more files in FDS File format. It is possible to
use it to load flow records previously stored using FDS output plugin.

Unlike UDP and TCP input plugins which infinitely waits for data from NetFlow/IPFIX
exporters, the plugin will terminate the collector after all files are processed.

Example configuration
---------------------

.. code-block:: xml

    <input>
        <name>FDS File</name>
        <plugin>fds</plugin>
        <params>
            <path>/tmp/flow/file.ipfix</path>
        </params>
    </input>

Parameters
----------

:``path``:
    Path to file(s) in IPFIX File format. It is possible to use asterisk instead of
    a filename/directory, tilde character (i.e. "~") instead of the home directory of
    the user, and brace expressions (i.e. "/tmp/{source1,source2}/file.ipfix").
    Directories and non-IPFIX Files that match the file pattern are skipped/ignored.

:``msgSize``:
    Maximum size of the internally generated IPFIX Messages to which the content
    of the file is inserted. [default: 32768, min: 512]

:``asyncIO``:
    Allows to use asynchronous I/O for reading the file. Usually when parts
    of the file are being read, the process is blocked on synchronous I/O
    and waits for the operation to complete. However, asynchronous I/O allows
    the plugin to simultaneously read the file and process flow records, which
    significantly improves overall performance. (Note: a pool of service
    threads shared among instances of FDS plugin might be created).
    [values: true/false, default: true]
