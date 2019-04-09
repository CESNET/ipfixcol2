IPFIX (output plugin)
=====================

The plugin outputs incoming IPFIX data to a file or a series of files in the IPFIX format. 

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