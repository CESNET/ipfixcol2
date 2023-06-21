ClickHouse (output plugin)
==========================

TODO...

How to build
------------

By default, the plugin is not distributed with IPFIXcol due to extra dependencies.
To build the plugin, IPFIXcol (and its header files) and the following dependencies must be
installed on your system:

TODO...

Finally, compile and install the plugin:

.. code-block:: sh

    $ mkdir build && cd build && cmake ..
    $ make
    # make install

Example configuration
---------------------

The following output instance configuration describes... TODO

.. code-block:: xml

    <output>
        <name>ClickHouse plugin</name>
        <plugin>clickhouse</plugin>
        <params>
            <connection>
                <host>localhost</host>
                <port>9000</port>
                <database>default</database>
                <user>default</user>
                <password></password>
            </connection>

            <table>
                <name>ipfixcol2</name>
                <createIfNotExists>true</createIfNotExists>
            </table>
        </params>
    </output>

Parameters
----------

TODO...
