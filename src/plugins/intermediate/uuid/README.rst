ASN enrichment (intermediate plugin)
===================================

The plugin performs enrichment of flow data with source and destination AS (Autonomous Systems) numbers.

Example configuration
---------------------

.. code-block:: xml

    <intermediate>
        <name>ASN Enrichment</name>
        <plugin>asn</plugin>
        <params>
            <path>GeoLite2-ASN.mmdb</path>
        </params>
    </intermediate>

Parameters
----------

:``path``:
    Path to the MaxMind GeoLite2 ASN database file. The file can be downloaded from
    `MaxMind <https://dev.maxmind.com/geoip/geoip2/geolite2/>`_.

.. Notes
.. -----