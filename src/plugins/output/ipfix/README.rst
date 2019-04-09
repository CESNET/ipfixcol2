IPFIX (output plugin)
=====================

The plugin outputs incoming IPFIX Data to an IPFIX File or a series of IPFIX Files. 

An IPFIX File is a serialized stream of IPFIX Messages. Simply put, the plugin stores all valid packets received by the collector into one or more IPFIX Files. Although this is not an optimal way to store flows, it can be quite useful for testing purposes - capture flow records into IPFIX File(s) and replay them using ipfixsend2 tool.

After a new file is started, all the (options) templates seen in the (options) template sets of an ODID that are still available are written to the file once the first IPFIX Message corresponding to the ODID arrives with at least one successfully parsed data record. This is necessary so each file can be used individually independent of the (options) template sets from previous files.    


Limitations
-----------
As there is no session information in raw IPFIX Data, we can't distinguish messages from multiple sessions using the same ODID. For this reason, the same ODID can only be used by one session at a time, and all messages from other sessions using the ODID that's already in use are ignored and a warning message is printed. All ODIDs used by a session are released once the session closes and they can be used by another session. 


Example configuration
---------------------

.. code-block:: xml

    <output>
        <name>IPFIX output</name>
        <plugin>ipfix</plugin>
        <params>
            <filename>/tmp/ipfix/%d-%m-%y/%H:%M:%S.ipfix</filename>
            <useLocalTime>false</useLocalTime>
            <windowSize>60</windowSize>
            <alignWindows>true</alignWindows>
            <skipUnknownDataSets>true</skipUnknownDataSets>
            <splitOnExportTime>false</splitOnExportTime>
        </params>
    </output>

Parameters
----------

:``filename``:
    The output filename, allows the use of strftime format values to add the export time into the file path.

:``useLocalTime``:
    Specifies whether the time values in filenames should be in local time instead of UTC. [default: false]

:``windowSize``:
    The number of seconds before a new file is created. The value of 0 means the file will never split. [default: 0]

:``alignWindows``:
    Specifies whether the file should only be split on multiples of windowSize. [default: true]

:``skipUnknownDataSets``:
    Specifies whether data sets with missing template should be left out from the output file. Sequence numbers and message lengths will be adjusted accordingly. [default: false]

:``splitOnExportTime``:
    Specifies whether files should be split based on export time instead of system time. [default: false]