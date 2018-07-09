IPFIXcol2
===========

IPFIXcol is a flexible, high-performance IPFIX flow data collector designed to be extensible
by plugins. The second generation of the collector includes many design and performance enhancements
compared to the original `IPFIXcol <https://github.com/CESNET/ipfixcol/>`_.

The collector allows you to choose combination of input, intermediate and output plugins that
best suit your needs. Do you need to receive data over UDP/TCP and store them for long term
preservation? Or, do you prefer conversion to JSON and processing by other systems?
No problem, pick any combination of plugins.

*Features:*

- Input, intermediate and output plugins with various options
- Parallelized design for high-performance
- Support for bidirectional flows (biflow)
- Built-in support for many Enterprise-Specific Information Elements (Cisco, Netscaler, etc.)

Available plugins
-----------------

**Input plugins** - receive IPFIX data. Each can be configured to to listen on a specific
network interface and a port. Multiple instances of these plugins can run concurrently.

- UDP - receives IPFIX over UDP
- TCP - receives IPFIX over TCP

**Intermediate plugins** - modify, enrich and filter flow records.

- anonymization - anonymize IP addresses (in flow records) with Crypto-PAn algorithm.

**Output plugins** - store or forward your flows.

- json - convert flow records to JSON and send/store them
- dummy - simple module example,
- lnfstore (*) - store all flows in nfdump compatible format for long-term preservation

\* Must be installed individually due to extra dependencies

How to build
------------

IPFIXcol is based on `libfds <https://github.com/CESNET/libfds/>`_ library that provides
functions for IPFIX parsing and manipulation. First of all, install the library.
For more information visit the project website and follow installation instructions.

However, you have to typically do following steps: (extra dependencies may be required)

.. code-block:: bash

    $ git clone https://github.com/CESNET/libfds.git
    $ cd libfds
    $ mkdir build && cd build && cmake ..
    $ make
    # make install

Second, install build dependencies of the collector

**RHEL/CentOS:**

.. code-block::

    yum install gcc gcc-c++ cmake make
    # Optionally: doxygen, pkg-config

* Note: latest systems (e.g. Fedora) use ``dnf`` instead of ``yum``.

**Debian/Ubuntu:**

.. code-block::

    apt-get install gcc g++ cmake make
    # Optionally: doxygen

Finally, build and install the collector:

.. code-block:: bash

    $ mkdir build && cd build && cmake ..
    $ make
    # make install

How to configure
----------------

TODO: Prepared configurations

TODO: example configuration files

TODO: description of information elements are


FAQ
--------------

Do you have any troubles? Unable to build and run the collector? *Feel free to submit a new issue.*

We are open to new ideas! For example, are you missing a specific plugin that could
be useful also for other users? Please, share your experience and thoughts.

----

:Q: How to...?
:A: You should...

----

:Q: How can I add more IPFIX fields into records?
:A: The collector receives flow records captured and prepared by an exporter. IPFIX is an
    unidirectional protocol which means that the collector is not able to instruct the exporter
    what to measure or how to behave. If you want to enhance your records, please, check
    configuration of your exporter.

Coming soon
-----------
- NetFlow v5/v9 support
- Runtime reconfiguration (improved compared to the previous generation)
- RPM/DEB packages
- Support for structured data types (lists, etc.)