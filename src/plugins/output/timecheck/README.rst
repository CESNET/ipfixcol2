Time Check (output plugin)
==========================

The plugin checks that start and end timestamps of each flow record are relatively recent.
Based on configured parameters it reports flows with timestamps from the future (i.e. greater
than the current system time) and timestamps from the distant past.

Timestamp anomalies are usually caused by missing clock synchronization (e.g. NTP) or invalid
implementation on side of the exporter. If the anomaly is detected, the plugin will print
details on the standard output.

Only following standard IANA timestamps are supported:

- ID 150 (flowStartSeconds)
- ID 151 (flowEndSeconds)
- ID 152 (flowStartMilliseconds)
- ID 153 (flowEndMilliseconds)
- ID 154 (flowStartMicroseconds)
- ID 155 (flowEndMicroseconds)
- ID 156 (flowStartNanoseconds)
- ID 157 (flowEndNanoseconds)

Example configuration
---------------------

.. code-block:: xml

    <output>
        <name>TimeCheck output</name>
        <plugin>timecheck</plugin>
        <params>
            <devPast>600</devPast>
            <devFuture>0</devFuture>
        </params>
    </output>

Parameters
----------

:``devPast``:
    Maximum allowed deviation between the current system time and timestamps from the past in
    seconds. The value must be greater than active and inactive timeouts of exporters and must also
    include a possible delay between expiration and processing on the collector.
    For example, let's say that active timeout and inactive timeout are 5 minutes and 30 seconds,
    respectively. Value 600 (i.e. 10 minutes) should be always enough for all flow data to be
    received and processed at the collector.

:``devFuture``:
    Maximum allowed deviation between the current time and timestamps from the future in seconds.
    The collector should never receive flows with timestamp from the future, therefore, the value
    should be usually set to 0.
