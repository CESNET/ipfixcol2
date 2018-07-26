Anonymization (intermediate plugin)
===================================

The plugin performs IPv4/IPv6 address anonymization of all flow records. There are two
available methods that could be applied on IP addresses, CryptoPAN and address truncation.

To identify IPFIX fields of a record to modify, the plugin uses a type of
an Information Element linked to each field. Thus, any record field with known
corresponding Information Element and type is always automatically anonymized.
Enterprise-specific Information Elements are supported too.

Example configuration
---------------------

.. code-block:: xml

    <intermediate>
        <name>Flow anonymization</name>
        <plugin>anonymization</plugin>
        <params>
            <type>CryptoPAn</type>
            <key>0123456789abcdefghijklmnopqrstuv</key>
        </params>
    </intermediate>

Parameters
----------

:``type``:
    Type of anonymization method. The string is case insensitive.

    :*CryptoPAn*:
        Cryptography-based sanitization and prefix-preserving method. The mapping from original
        IP addresses to anonymized IP addresses is one-to-one and if two original IP addresses
        share a k-bit prefix, their anonymized mappings will also share a k-bit prefix.
        Be aware that this cryptography method is very demanding and can limit throughput
        of the collector.

    :*Truncation*:
        This method keeps the top part and erases the bottom part of an IP address. Compared
        to the CryptoPAn method, it is considerably faster, however, mapping from the original
        to anonymized IP address is many-to-one. For example the IPv4 address "1.2.3.4" is
        mapped to the address "1.2.0.0".

:``key``:
    Optional cryptography key for CryptoPAn anonymization. The length of the string must be exactly
    32 bytes. If the key is not specified, a random one is generated during the initialization.

Notes
-----

Usually all common IP addresses are automatically anonymized. However, if an IPFIX field is not,
make sure that the particular Information Element is
defined among other definitions provided by `libfds <https://github.com/CESNET/libfds/>`_ library.
Mainly in case of Enterprise-Specific Information Elements, there is a chance that the
definitions are missing. See the documentation of the library, for help to easily add extra
definitions in few steps.
