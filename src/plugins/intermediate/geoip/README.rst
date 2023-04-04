GeoIP enrichment (intermediate plugin)
===================================

The plugin performs enrichment of flow data with geographical information such as:

:``CountryCode``:
	Alpha-2 code of destination and source countries for given flow.

:``Latitude``:
	Latitude of flow origins.

:``Longitude``:
	Longitude of flow origins.

Example configuration
---------------------

.. code-block:: xml

    <intermediate>
        <name>GeoIP Enrichment</name>
        <plugin>geoip</plugin>
        <params>
            <path>GeoLite2-City.mmdb</path>
        </params>
    </intermediate>

Parameters
----------

:``path``:
    Path to the MaxMind GeoLite2 ASN database file. The file can be downloaded from
    `MaxMind <https://dev.maxmind.com/geoip/geoip2/geolite2/>`_.

.. Notes
.. -----