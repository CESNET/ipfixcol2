Overview output plugin
======================

Provides an overview of elements that the collector is receiving.

This can be useful if you need to quickly figure out the IPFIX fields your flow
probe is sending to the collector.

The overview is printed to stdout upon exit as a JSON.

Example output
--------------

.. code-block:: json

    {
        "total_number_of_records": 24985802,
        "elements": [
            {
                "pen": 0,
                "id": 136,
                "data_type": "unsigned8",
                "name": "iana:flowEndReason",
                "aliases": [ ],
                "in_number_of_records": 1112239,
                "in_percent_of_records": 4.45
            },
            {
                "pen": 0,
                "id": 1,
                "data_type": "unsigned64",
                "name": "iana:octetDeltaCount",
                "aliases": [ "bytes" ],
                "in_number_of_records": 1112239,
                "in_percent_of_records": 4.45
            },
            ...
        ]
    }


If a certain field only has a `pen` and `id` set with certain fields such as
`name` being `null`, it means that the collector is missing a definition of the
field. Field definitions are defined as XML files and can be by default found
in `/etc/libfds/` if ipfixcol2 is installed system-wide.

Definitions that come with ipfixcol2 are stored in `/etc/libfds/system`
directory. Your own definitions can be supplied in the `/etc/libfds/user`
directory. See one of the files in `/etc/libfds/system` for the structure of
the XML files.


Example configuration
---------------------

.. code-block:: xml

    <output>
        <name>Overview output</name>
        <plugin>overview</plugin>
        <params>
            <skipOptionTemplates>false</skipOptionTemplates> <!-- default value -->
        </params>
    </output>

Parameters
----------

:``skipOptionsTemplates``:
    Options templates and options template records are skipped. [default: false]
