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
            <useUtcForFilenameTime>false</useUtcForFilenameTime>
            <windowSize>60</windowSize>
            <alignWindows>true</alignWindows>
            <skipUnknownDataSets>true</skipUnknownDataSets>
        </params>
    </output>

Parameters
----------

:``filename``:
    The output filename, allows the use of strftime format values to add the export time into the file path.

:``useUtcForFilenameTime``:
    Specifies whether the time values in filenames should be in local time or UTC. (optional, local time by default)

:``windowSize``:
    The number of seconds before a new file is created. The value of 0 means the file will never split. (optional, 0 by default)

:``alignWindows``:
    Specifies whether the file should only be split on multiples of windowSize. (optional, true by default)

:``skipUnknownDataSets``:
    Specifies whether data sets with missing template should be left out from the output file. Sequence numbers and message lengths will be adjusted accordingly. (optional, false by default)
