IPFIX File (input plugin)
=========================

The plugin reads flow data from one or more files in IPFIX File format. It is possible to
use it to load flow records previously stored using IPFIX output plugin.

Unlike UDP and TCP input plugins which infinitely waits for data from NetFlow/IPFIX
exporters, the plugin will terminate the collector after all files are processed.

Example configuration
---------------------

.. code-block:: xml

    <input>
        <name>IPFIX File</name>
        <plugin>ipfix</plugin>
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

:``bufferSize``:
    Optional size of the internal buffer to which the content of the file is partly
    preloaded. [default: 1048576, min: 131072]
