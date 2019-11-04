IPFIX (output plugin)
=====================

The plugin writes incoming IPFIX Messages to an IPFIX File or a series of IPFIX
Files.

An IPFIX File is a serialized stream of IPFIX Messages. Simply put, the plugin
stores all valid packets received by the collector into one or more IPFIX Files.
Although this is not an optimal way to store flows, it can be quite useful
for testing purposes - capture flow records into IPFIX File(s) and replay them
using ``ipfixsend2`` tool.

Limitations
-----------

In a situation when multiple exporters are using the same ODID at a time, the
file format can store flow records only from one of the conflicting exporters
with the given ODID.

This is due to fact that the IPFIX Data Records (i.e. flow records) are
formatted based on so called (Options) Template definitions (i.e. descriptions
of flow record structures), which are unique for combination of a Transport
Session (e.g. connection to a flow exporter) and an ODID. As there is no session
information in raw IPFIX Messages, we can't distinguish Messages from multiple
sessions using the same ODID. For this reason, each ODID can be used only by
one session at a time, and all messages from other sessions using the
same ODID that's already in use are ignored and a warning message is shown.
Once a Transport Session is closed, ODIDs used by the session are released
and can be later reused by another session.

Example configuration
---------------------

.. code-block:: xml

    <output>
        <name>IPFIX output</name>
        <plugin>ipfix</plugin>
        <params>
            <filename>/tmp/ipfix/%Y%m%d%H%M%S.ipfix</filename>
            <useLocalTime>false</useLocalTime>
            <windowSize>300</windowSize>
            <alignWindows>true</alignWindows>
            <preserveOriginal>false</preserveOriginal>
            <rotateOnExportTime>false</rotateOnExportTime>
        </params>
    </output>

Parameters
----------

:``filename``:
    Specifies an absolute path of output files. The path should contain format
    specifier for day, month, etc. For example "/tmp/ipfix/%Y%m%d%H%M%S.ipfix".
    See ``strftime``\ (3) for more information.

:``useLocalTime``:
    Specifies whether the time values in filenames should be in local time
    instead of UTC. [default: false]

:``windowSize``:
    Specifies the time interval in seconds to rotate files. If the value
    is "0", all flow will be stored into a single file. [default: 0]

:``alignWindows``:
    Align file rotation with next N minute interval. [values: true/false,
    default: true]

:``preserveOriginal``:
    Preserve and store original IPFIX Messages as received by the plugin i.e.
    do not modify the Messages or their sequence numbers. If this option is
    disabled (recommended), Data Sets with unknown (Options) Templates are
    left out from the file and sequence numbers are adjusted accordingly
    (starts from 0), which should produce warning-free stream of IPFIX
    Messages. However, if the original IPFIX Messages are preserved, some
    warnings (e.g. missing (Options) Templates, unexpected sequence number,
    etc.) might be produced during replay and even errors might raise when
    there is an ODID collision and the main Transport Session is replaced.
    Use with caution. [values: true/false, default: false]

:``rotateOnExportTime``:
    Specifies whether files should be rotated based on IPFIX Export Time
    (i.e. timestamp from IPFIX Message header) instead of system time.
    If enabled, the Export Time is used also for creating filenames.
    Warning: If the plugin receives flow records from multiple exporters at
    time the rotation could be unsteady. [default: false]

Note
----

After a new IPFIX File is created, all previously seen and still valid (Options)
Templates of the each ODID are written to the file once the first IPFIX Message
corresponding to the ODID arrives with at least one successfully parsed data
record. This is necessary so each file can be used independently of the
(Options) Template definitions from previous files.

``ipfixsend2`` tool doesn't support sequential reading of multiple IPFIX Files
right now. However, there is a workaround - you can merge multiple IPFIX Files
using cat tool e.g. ``cat file1.ipfix file2.ipfix > merge.ipfix`` and then use
``ipfixsend2 -i merge.ipfix`` to send data.
