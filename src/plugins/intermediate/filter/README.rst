Filter (intermediate plugin)
============================

The plugin performs filtering of flow records based on an filter expression.
Flow records not matching the specified filtering criteria are discarded.


Supported operations
--------------------

 - Comparison operators `==`, `<`, `>`, `<=`, `>=`, `!=`. If the comparison operator is ommited, the default comparison is `==`.

 - The `contains` operator for substring comparison, e.g. `DNSName contains "example"`.

 - Arithmetic operations `+`, `-`, `*`, `/`, `%`.

 - Bitwise operations not `~`, or `|`, and `&`, xor `^`.

 - The `in` operator for list comparison, e.g. `port in [80, 443]`.

 - The logical `and`, `or`, `not` operators.


Value types
-----------

 - Numbers can be integer or floating point. Integer numbers can also be written in their hexadecimal or binary form using the `0x` or `0b` prefix.
   Floating point numbers also support the exponential notation such as `1.2345e+2`. A number can be explicitly unsigned using the `u` suffix.
   Numbers also support size suffixes `B`, `k`, `M`, `G`, `T`, and time suffixes `ns`, `us`, `ms`, `s`, `m`, `d`.

 - Strings are values enclosed in a pair of double quotes `"`. Supported escape sequences are `\n`, `\r`, `\t` and `\"`.
   The escape sequences to write characters using their octal or hexadecimal value are also supported, e.g. `\ux22` or `\042`.

 - IP addresses are written in their usual format, e.g. `127.0.0.1` or `1234:5678:9abc:def1:2345:6789:abcd:ef12`. The shortened IPv6 version is also supported, e.g. `::ff`.
   IP addresses can also contain a suffix specifying their prefix length, e.g. `10.0.0.0/16`.

 - MAC addresses are written in their usual format, e.g. `12:34:56:78:9a:bc`.

 - Timestamps use the ISO timestamp format, e.g. `2020-04-05T24:00Z`.


IPFIX field identificators
--------------------------

IPFIX fields can be identified using their name specified in the IPFIX information elements table or their alias defined in the `aliases.xml` file.
If the IPFIX name is used and the default iana table is being referred, the `iana:` prefix can be ommited.
Note that one alias can point to multiple IPFIX information elements.
The default location of the aliases file is `/etc/libfds/system/aliases.xml`.


Value mappings
--------------

Commonly used values can be mapped to a name using the `mappings.xml` file, for example the name `http` when used in an expression `port http` can refer to the value 80.
These names can have different meanings depending on the IPFIX field they're being compared with.
The default location of the mappings file is `/etc/libfds/system/mappings.xml`.


Example configuration
---------------------

.. code-block:: xml

    <intermediate>
        <name>Filter</name>
        <plugin>filter</plugin>
        <params>
            <expr>ip 10.0.0.0/16 and port in [80, 8080]</expr>
        </params>
    </intermediate>

The most common use case would be filtering based on a list of allowed IP
address ranges. In such case, it is recommended to use the following construct
for optimal performance:

.. code-block:: xml

    <intermediate>
        <name>Filter</name>
        <plugin>filter</plugin>
        <params>
            <expr>srcip in [1.0.0.0/8, 2.2.0.0/16, 3.3.3.0/24] or dstip in [4.4.4.0/24]</expr>
        </params>
    </intermediate>


Parameters
----------

:``expr``:
    The filter expression.
