fdsdump
========

A nfdump-like tool for working with FDS files.

Parameters
-----------

:``-h``:
    Show help message and exit.

:``-r FILE``:
    File to read from. Can be specified multiple times. FILE can also be a glob pattern matching multiple files.

:``-f FILTER``:
    Input filter, e.g. ``ip 10.0.0.0/16 and port 443`` or ``flowmon.dnsQname contains ".com"``.

:``-a KEYS``:
    Fields to aggregate on.

    Supported values: ``srcipv4``, ``dstipv4``, ``srcipv6``, ``dstipv6``, ``ipv4``, ``ipv6``, ``srcip``, ``dstip``, ``srcport``, ``dstport``, ``proto``, ``ip``, ``port``, ``biflowdir``, or a name of an IPFIX field.

    IP address values with a version can also contain a prefix length, e.g. ``ipv4/24`` or ``srcipv6/64``.

:``-s VALUES``:
    Fields to aggregate.

    Supported values: ``packets``, ``bytes``, ``flows``, ``inpackets``, ``outpackets``, ``inbytes``, ``outbytes``, ``inflows``, ``outflows``, or a name of an IPFIX field with an aggregation function specified.

    Possible aggregation functions are: ``min``, ``max``, ``sum``.

    The aggregation function is specified using a suffix, e.g. ``octetDeltaCount:sum`` or ``flowStartMilliseconds:min``.

    Fields are seperated by a comma, e.g. ``packes,bytes,flows,flowStartMilliseconds:min,flowStartMilliseconds:max``.

:``-O FIELDS``:
    Fields to order by.

    Supported values: field names as specified in ``-s``.

    Order is descending by default and can be specified by a ``/asc`` or ``/desc`` suffix, e.g. ``packets/asc``.

    Multiple fields can be specified and are seperated by a comma, e.g. ``packets,bytes``.

:``-F FILTER``:
    Output (aggregate) filter, e.g. ``packets > 1000``.

:``-n NUM``:
    Maximum number of output records (i.e. top N).

:``-t NUM``:
    Number of threads to use.

:``-d``:
    Attempt to translate IP addresses to domain names when printing.

:``-o MODE``:
    Output mode/format.

    Possible values: ``table``, ``json``, ``csv``.

    Default: ``table``.

Examples
---------

.. code:: bash
    ``fdsdump -r 'data/``*``.fds' -f 'ip 80.0.0.0/8 or ip 81.0.0.0/8' -a biflowdir,srcip,srcport,dstip,dstport,proto -s packets -n 50 -O packets -F 'packets > 10000' -t 8``