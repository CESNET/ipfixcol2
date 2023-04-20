Dummy enrichment (intermediate plugin)
===================================

The plugin performs enrichment of flow data with configured dummy values. It is used for testing purposes.

Example configuration
---------------------

.. code-block:: xml

    <intermediate>
        <name>Dummy enricher</name>
        <plugin>dummy_enrich</plugin>
        <params>
            <field>
                <pen>0</pen>
                <id>82</id> // Interface name
                <type>string</type>
                <str_val>enp0s3</str_val>
            </field>
            <field>
                <pen>0</pen>
                <id>86</id> // Total packets cnt
                <type>integer</type>
                <int_val>65980</int_val>
                <times>2</times>
            </field>
        </params>
    </intermediate>

Parameters
----------

:``field``:
    Definition of added fields. Each field is defined by following parameters:

    :``pen``:
        Private Enterprice number of the field.

    :``id``:
        Field ID.

    :``type``:
        Type of field [supported values: ``string``, ``integer``, ``double``]. Depending on type of field, one of the following value fields must be used:


.. Notes
.. -----