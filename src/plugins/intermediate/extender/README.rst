============================
 Extender (intermediate plugin)
============================

This intermediate plugin extends IPFIX records by adding new items to the existing record set using user-defined expressions.


Example configuration
---------------------

.. code-block:: xml

    <intermediate>
      <name>Extenders</name>
      <plugin>extender</plugin>
      <params>
        <ids>          
          <id>iana:VRFname</id>
          <values>
            <expr>iana:dot1qCustomerVlanId == 0</expr>
            <value>vrf-0</value>
          </values>
          <values>
            <expr>iana:dot1qCustomerVlanId == 1</expr>
            <value>vrf-1</value>
          </values>
        </ids>
      </params>
    </intermediate>

The above sample adds a new string field ``VRFname=vrf-0`` or ``VRFname=vrf-1`` to each IPFIX record depending on the value of ``dot1qCustomerVlanId``.

Parameters
----------

``expr``
    The filter expression to evaluate.

``id``
    The identifier string to be added to the record when the expression evaluates to true.


Supported operations
--------------------

Comparisons
    - Operators: ``==``, ``<``, ``>``, ``<=``, ``>=``, ``!=``
    - Default: ``==`` (if the operator is omitted)

Substring Matching
    - Operator: ``contains``
    - Example: ``DNSName contains "example"``

Arithmetic
    - Operators: ``+``, ``-``, ``*``, ``/``, ``%``

Bitwise Logic
    - Operators: ``~`` (not), ``|`` (or), ``&`` (and), ``^`` (xor)

List Membership
    - Operator: ``in``
    - Example: ``port in [80, 443]``

Logical Operators
    - Operators: ``and``, ``or``, ``not``