Collector configuration
=======================

.. image:: img/configuration.svg
    :width: 60%
    :align: right

IPFIXcol is designed to be easily extensible by input, intermediate and output plugins.
Therefore, the startup configuration of the collector consists of 3 parts, each for one type of
plugins, as illustrated below. The collector typically receives data from exporters and stores
them on local drive or forward them to another destination. Hence, every startup configuration
contains description of at least one instance of an input plugin and one instance of an output
plugin. The intermediate section can be omitted, if not required.

.. code-block:: xml

    <ipfixcol2>
        <inputPlugins>
            ...
        </inputPlugins>

        <intermediatePlugins>
            ...
        </intermediatePlugins>

        <outputPlugins>
            ...
        </outputPlugins>
    </ipfixcol2>

A plugin documentation always provides description of parameters and configuration snippet
that can be easily copied into your configuration. However, keep in mind that some parameters
should be modified, for example, a storage directory of an output plugin, etc.

Input plugins
-------------

The main purpose of the collector is to process flow data. To receive and process them,
input plugins are essential. Thus, one or more instances of input plugins must defined within
``<inputPlugins>`` section. First, let's look at what is composed of:

.. code-block:: xml

    <input>
        <name>...</name>
        <plugin>...</plugin>
        <params>...</params>
    </input>

:``name``:
    User identification of the instance. Must be unique within the section.
:``plugin``:
    Internal identification of a plugin to whom the instance belongs. The identification
    is assigned by an author of the plugin.
:``params``:
    Configuration parameters of the instance. The parameters varies from plugin to plugin.
    For help, see documentation of individual plugins.

For example, this is how a configuration of `TCP <../../src/plugins/input/tcp>`_ input plugin
could look like:

.. code-block:: xml

    <input>
        <name>TCP collector</name>
        <plugin>tcp</plugin>
        <params>
            <localPort>4739</localPort>
            <localIPAddress></localIPAddress>
        </params>
    </input>

Intermediate plugins
--------------------

Now we have flow data inside the collector, but what if we want to modify or enrich them.
Intermediate plugins are here to help. On the other hand, if modifications are not
required, the whole section can be omitted. Thus, zero or more instances of intermediate
plugins can be defined inside ``<intermediatePlugins>`` section.

Although the order of input and output instances in a configuration doesn't matter, in case of
intermediate plugins, it is important because it represents order of flow processing inside
the internal data pipeline.

Let's look at what the definition of an intermediate instance is composed of:

.. code-block:: xml

    <intermediatePlugins>
        <name>...</name>
        <plugin>...</plugin>
        <params>...</params>
    </intermediatePlugins>

As you can see, the structure of configuration is almost equivalent to the structure of
input instances. They differ only in the element name, but parameter meaning is still the same.

Output plugins
--------------

Flow records are already prepared by input and intermediate plugins. The next step is to store them
on local drive or forward to another destination for further processing. For these reasons,
one or more instances of output plugins must be defined inside ``<outputPlugins>`` section.
Again, the structure of an instance definition looks pretty similar like before.

.. code-block:: xml

    <output>
        <name>...</name>
        <plugin>...</plugin>
        <params>...</params>
    </output>

By default, an instance processes all records that are received by input plugins. Howerever, each
output instance also supports *optional* Observation Domain ID (ODID) filter.
What does it mean for you? Let's say you have multiple exporters monitoring your network.
These exporters typically allows you to set an ODID associated to exported flow records so
you can easily distinguish their origin. On the collector side, the ODID filter of an output
instance allows you to select a range of ODIDs that should be processed by the particular instance.
Flows from other sources are ignored.

How can you use it? One of many common use-cases is that if you want to store flow data
from different exporters to different output directories you can create multiple instances
of the same output plugin with similar configurations and different ODID filters.
Another use-case that is also worth mentioning is load-balancing. For example, when
conversion of flow to JSON is not fast enough, you can try to split flows into multiple
groups based on their ODID and process each group by an independent instance of the plugin.

To enable the optional ODID filter, use one of the following parameter that takes a filter
expression as an argument:

:``<odidOnly>``:   Process flows only from the selected ODID range
:``<odidExcept>``: Process all flows except those from the selected ODID range

The filter expression is represented as comma separated list of unsigned numbers
and intervals. Interval is all the numbers between two given numbers separated by a dash.
If one number of the interval is missing, the minimum or the maximum is used by default.
For example, "1-5, 7, 10-" represents all ODIDs except 0, 6, 8 and 9

.. code-block:: xml

    <output>
        ...
        <odidOnly>...</odidOnly>
        <!-- or -->
        <odidExcept>...</odidExcept>
        ...
    </output>

See documentation of your exporters how to configure exported ODID. It is recommended that
ODIDs are unique per exporter. Note: In case of NetFlow devices, ODID is often referred as
"Source ID".

Example configuration files
---------------------------


Multiple instance of different input plugins
Multiple instances of the same plugin processing different ODID range (load balancing)





Try your configuration
----------------------

TOOD: how to start collector with a configuraton
- just call ``ipfixcol2 -c <config_file>``.

TODO: if your configuration is ready to use, you can also store to the default path
where the collector looks for it... often it is /etc/ipfixcol/startup.xml
however based on your OS and build configuration of the collector path can vary.
Use ``ipfixcol2 -h`` to see default paths on your system. Then you can start collector
by calling only ``ipfixcol2`` (without parameters)





Verbosity
---------



TODO: order of input/output plugin doesn't depend... order of intermediate plugins is significant
TODO: verbosity level
TODO: odid filter
TODO: multiple instances of the same plugin can be used... for example TCP, UDP, etc..
TODO: where the collector search for plugins??

The fundamental objective of the collector is an emphasis on high performance. To reach this
goal, each instance is executed by its own thread. This makes possible to run simultaneously
multiple input, intermediate and output instances and at the same effectively use system resources.

Even instances of the same plugin can run concurrently. However, instances must have slightly
different configurations. For example, instances of output plugins should not write to the same
directory, instances of input plugins cannot listen on the same port and interface, etc.

Just like everywhere else your collector is as fast as the slowest plugin in the configuration.
