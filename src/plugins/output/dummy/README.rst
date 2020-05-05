Dummy (output plugin)
=====================

The plugin provides a means to test the collector without actually storing any data.
Every processing of an IPFIX packet can have specified delay so that throughput of
the collector can be tested and required performance of output plugins can be determined.

Example configuration
---------------------

.. code-block:: xml

    <output>
        <name>Dummy output</name>
        <plugin>dummy</plugin>
        <params>
            <delay>0</delay>
            <stats>true</stats>
        </params>
    </output>

Parameters
----------

:``delay``:
    Minimum delay between processing of two consecutive messages in microseconds.

:``stats``:
    Print basic statistics after termination (flows, bytes, packets).
    [values: true/false, default: false]