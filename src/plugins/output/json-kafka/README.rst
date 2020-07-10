JSON Kafka (output plugin)
==========================

    Special version of JSON output plugin for Apache Kafka. The plugin is copy of
    the original plugin so it offers the same formatting options, however, it provides
    only Kafka output.

    See "Kafka notes" section below for more information.

The plugin converts IPFIX flow records into JSON format and sends them to Apache Kafka.
Some elements (timestamps, IP/MAC addresses, TCP flags, etc.) can be converted into human
readable format. Others are represented as plain numbers.

Each flow record is converted individually and send to outputs where at least one output
method must be configured. All fields in an IPFIX record are formatted as: <field name>:<value>,
where the *field name* represents a name of a corresponding Information Element as
"<Enterprise Name>:<Information Element Name>" and *value* is a number, string,
boolean or null (if conversion from IPFIX to JSON fails or it is a zero-length field). The
Enterprise Name is a name of an organization that manages the field, such as IANA, CISCO, etc.

If the field name and type is unknown and "ignoreUnknown" option is disabled, the field name
use format 'enXX:idYY' where XX is an Enterprise Number and YY is an Information Element ID.
See notes for instructions on how to add missing definitions of Information Elements.

Flow records with structured data types (basicList, subTemplateList and subTemplateMultiList) are
supported. See the particular section below for information about the formatting of these types.

Example output
--------------

The following output is for illustration purpose only. Available IPFIX fields depend on
a structure of records that your exporter sends. See documentation of the exporter,
if you want to change available IPFIX fields.

.. code-block:: json

    {
        "@type": "ipfix.entry",
        "iana:octetDeltaCount": 3568720,
        "iana:packetDeltaCount": 2383,
        "iana:flowStartMilliseconds": "2018-05-11T19:44:29.006Z",
        "iana:flowEndMilliseconds": "2018-05-11T19:44:30.364Z",
        "iana:ingressInterface": 1,
        "iana:ipVersion": 4,
        "iana:sourceIPv4Address": "192.168.0.100",
        "iana:destinationIPv4Address": "192.168.0.101",
        "iana:ipClassOfService": 0,
        "iana:ipTTL": 62,
        "iana:protocolIdentifier": "TCP",
        "iana:tcpControlBits": ".AP.S.",
        "iana:sourceTransportPort": 47200,
        "iana:destinationTransportPort": 20810,
        "iana:egressInterface": 0,
        "iana:samplingInterval": 0,
        "iana:samplingAlgorithm": 0
    }

The plugin can produce multiple record types, which are always distinguished by "@type" key:

:``ipfix.entry``:
    Flow record captured by the exporter as shown in the example above. This record is always
    converted and it is the most common type.
:``ipfix.optionsEntry``:
    Record containing additional information provided by the exporter to the collector. Usually
    it is used for information about the exporter configuration (such as packet sampling)
    and flow export statistics. Periodicity and content of the record always depends on the
    particular exporter. By default, record conversion is disabled, see ``ignoreOptions`` parameter
    in the configuration section.
:``ipfix.template`` and ``ipfix.optionsTemplate``:
    Records describing structure of flow records (i.e. ``ipfix.entry``) and options records
    (i.e. ``ipfix.optionsEntry``), respectively. These types are usually useful only for
    exporter analytics. Periodicity of these records usually depends on the exporter and used
    transport protocol. For example, for connection over UDP it corresponds to
    "template refresh interval" configured on the exporter. However, for TCP and SCTP sessions,
    the template records are usually sent only once immediately after session start.
    By default, template record conversion is disabled, see ``templateInfo`` parameter in the
    configuration section.

Example configuration
---------------------

Below you see a complex configuration with all available output options enabled.
Don't forget to remove (or comment) outputs that you don't want to use!

.. code-block:: xml

    <output>
        <name>JSON output</name>
        <plugin>json-kafka</plugin>
        <params>
            <tcpFlags>formatted</tcpFlags>
            <timestamp>formatted</timestamp>
            <protocol>formatted</protocol>
            <ignoreUnknown>true</ignoreUnknown>
            <ignoreOptions>true</ignoreOptions>
            <nonPrintableChar>true</nonPrintableChar>
            <octetArrayAsUint>true</octetArrayAsUint>
            <numericNames>false</numericNames>
            <splitBiflow>false</splitBiflow>
            <detailedInfo>false</detailedInfo>
            <templateInfo>false</templateInfo>

            <outputs>
                <kafka>
                    <name>Send to Kafka</name>
                    <brokers>127.0.0.1</brokers>
                    <topic>ipfix</topic>
                    <blocking>false</blocking>
                    <partition>unassigned</partition>

                    <!-- Zero or more additional properties -->
                    <property>
                        <key>compression.codec</key>
                        <value>lz4</value>
                    </property>
                </kafka>
            </outputs>
        </params>
    </output>

Parameters
----------

Formatting parameters:

:``tcpFlags``:
    Convert TCP flags to common textual representation (formatted, e.g. ".A..S.")
    or to a number (raw). [values: formatted/raw, default: formatted]

:``timestamp``:
    Convert timestamp to ISO 8601 textual representation (all timestamps in UTC and milliseconds,
    e.g. "2018-01-22T09:29:57.828Z") or to a unix timestamp (all timestamps in milliseconds).
    [values: formatted/unix, default: formatted]

:``protocol``:
    Convert protocol identification to formatted style (e.g. instead 6 writes "TCP") or to a number.
    [values: formatted/raw, default: formatted]

:``ignoreUnknown``:
    Skip unknown Information Elements (i.e. record fields with unknown name and data type).
    If disabled, data of unknown elements are formatted as unsigned integer or hexadecimal values.
    For more information, see ``octetArrayAsUint`` option. [values: true/false, default: true]

:``ignoreOptions``:
    Skip non-flow records used for reporting metadata about IPFIX Exporting and Metering Processes
    (i.e. records described by Options Templates). [values: true/false, default: true]

:``nonPrintableChar``:
    Ignore non-printable characters (newline, tab, control characters, etc.) in IPFIX strings.
    If disabled, these characters are escaped on output. [values: true/false, default: true]

:``octetArrayAsUint``:
    Converter each IPFIX field with octetArray type (including IPFIX fields with unknown
    definitions) as unsigned integer if the size of the field is less or equal to 8 bytes.
    Fields with the size above the limit are always converted as string representing hexadecimal
    value, which is typically in network byte order (e.g. "0x71E1"). Keep on mind, that there might
    be octetArray fields with variable length that might be interpreted differently based on their
    size. If disabled, octetArray fields are never interpreted as unsigned integers.
    [values: true/false, default: true]

:``numericNames``:
    Use only short identification of Information Elements (i.e. "enXX:idYY"). If enabled, the
    short version is used even if the definition of the field is known. This option can help to
    create a shorter JSON records with key identifiers which are independent on the internal
    configuration. [values: true/false, default: false]

:``splitBiflow``:
    In case of Biflow records, split the record to two unidirectional flow records. Non-biflow
    records are unaffected. [values: true/false, default: false]

:``detailedInfo``:
    Add additional information about the IPFIX message (such as export time, Observation Domain ID,
    IP address of the exporter, etc.) to which each record belongs. Additional fields starts
    with "ipfix:" prefix. [values: true/false, default: false]

:``templateInfo``:
    Convert Template and Options Template records. See the particular section below for
    information about the formatting of these records. [values: true/false, default: false]

----

Output types: At least one output must be configured. Multiple kafka outputs can be used
at the same time if the outputs are not in collision with each other.

:``kafka``:
    Send data to Kafka i.e. Kafka producer.

    :``name``: Identification name of the output. Used only for readability.
    :``brokers``:
        Initial list of brokers as a CSV list of broker "host" or "host:port".
    :``topic``:
        Kafka topic to produce to.
    :``partition``:
        Partition number to produce to. If the value is "unassigned", then the default random
        distribution is used. [default: "unassigned"]
    :``brokerVersion``:
        Older broker versions (before 0.10.0) provide no way for a client to query for
        supported protocol features making it impossible for the client to know what features
        it may use. As a workaround a user may set this property to the expected broker
        version and the client will automatically adjust its feature set.
        [default: <empty>]
    :``blocking``:
        Enable blocking on produce. If disabled and a cluster is down or not able
        to retrieve records fast enough, some flow records may be dropped. On the other hand,
        if enabled, no records are dropped. However, if the cluster is slow or not accessible
        at all, the plugin waits (i.e. blocks) until data are send. This can significantly slow
        down or block(!) the whole collector and other output plugins [true/false, default: false]
    :``performanceTuning``:
        By default, the connection provided by librdkafka is not optimized for high throughput
        required for transport of JSON records. This option adds optional library parameters,
        which reduces messaging overhead and significantly improves throughput. In particular,
        Kafka message capacity is increased and maximal buffering interval is prolonged.
        These options can be overwritten by user defined properties.
        [true/false, default: true]
    :``property``:
        Additional configuration properties of librdkafka library as key/value pairs.
        Multiple <property> parameters, which can improve performance, can be defined.
        See the project website for the full list of supported options. Keep on mind that
        some options might not be available in all versions of the library.

Notes
-----

If one or more Information Element definitions are missing, you can easily add them.
All definitions are provided by `libfds <https://github.com/CESNET/libfds/>`_ library.
See the project website for help.

If a flow record contains multiple occurrences of the same Information Element,
their values are stored into a single name/value pair as JSON array. Order of the values
in the array corresponds to their order in the flow record.

For higher performance, it is advisable to use non-formatted conversion of IPFIX data types.
In that case, you should prefer, for example, timestamps as numbers over ISO 8601 strings
and numeric identifiers of fields as they are usually shorted.

Structured data types
---------------------

Flow records can be extended with structured data types (as described in RFC6313).
Each of these types are formatted as JSON objects with "@type" field which helps to distinguish
its formatting. Moreover, as the standard describes, the semantic of the list is also included.

Converted *basicList* contains "fieldID" with the Information Element identifier of zero or more
Information Element(s) contained in the list. All values are stored as a JSON array in "data" field.

.. code-block:: json

    {
        "example:blField": {
            "@type": "basicList",
            "semantic": "anyOf",
            "fieldID": "iana:octetDeltaCount",
            "data": [23, 34, 23]
        }
    }

Converted *subTemplateList* contains only additional "data" field with array of zero or more
JSON objects. As all nested JSON object are described by the same IPFIX Template, it's guaranteed
the their structure is also the same. The "semantic" field indicates the relationship among the
different JSON objects.

.. code-block:: json

    {
        "example:stlField": {
            "@type": "subTemplateList",
            "semantic": "allOf",
            "data": [
                {"keyA.1": "value1", "keyA.2": "value2"},
                {"keyB.1": "value1", "keyB.2": "value2"},
            ]
        }
    }

Converted *subTemplateMultiList* is similar to the previous type, however, sub-records can be
even more nested. The "data" field contains a JSON array with zero or more nested JSON arrays.
Each nested array contains zero or more JSON objects and it's guaranteed that JSON objects in
the same array have the same structure. The "semantic" field indicates top-level relationship
among the nested arrays.

.. code-block:: json

    {
        "example:stmlField": {
            "@type": "subTemplateMultiList",
            "semantic": "allOf",
            "data" : [
                [
                    {"keyA.1": "value1", "keyA.2": "value2"},
                    {"keyB.1": "value1", "keyB.2": "value2"},
                ],
                [
                    {"idA.1": "something", "idB.1": 123}
                ]
            ]
        }
    }

Keep on mind that all structures can be nested in each other.

Template and Options Template records
-------------------------------------

These records are converted only if ``<templateInfo>`` option is enabled.

*Template record* describes structure of flow records, and its "@type" is "ipfix.template".
Converted Template record contains "ipfix:templateId" field, which is unique to the Transport
Session and Observation Domain, and "ipfix:fields", which is an array of JSON objects specifying
fields of flow records.

.. code-block:: json

    {
        "@type": "ipfix.template",
        "ipfix:templateId": 49171,
        "ipfix:fields": [
            {
                "ipfix:elementId": 16399,
                "ipfix:enterpriseId": 6871,
                "ipfix:fieldLength": 1
            }, {
                "ipfix:elementId": 184,
                "ipfix:enterpriseId": 29305,
                "ipfix:fieldLength": 4
            }, {
                ...
            }
        ]
    }

*Options Template record* describes structure of additional information for the collector, such as
sampling configuration, export statistics, etc. The description is converted to a record with
"@type" equal to "ipfix.optionsEntry". Converted Options Template record is similar
to the previous type, however, it contains additional "ipfix:scopeCount" field, which specifies
number of scope fields in the record. A scope count of N specifies that the first N field
specifiers are scope fields.

.. code-block:: json

    {
        "@type": "ipfix.optionsTemplate",
	    "ipfix:templateId": 53252,
	    "ipfix:scopeCount": 1,
	    "ipfix:fields": [
	        {
		        "ipfix:elementId": 130,
		        "ipfix:enterpriseId": 0,
		        "ipfix:fieldLength": 4
	        }, {
		        "ipfix:elementId": 103,
		        "ipfix:enterpriseId": 6871,
		        "ipfix:fieldLength": 4
	        }, {
                ...
            }
        ]
    }

Kafka notes
-----------

The plugin depends on ``librdkafka`` which unfortunately redefines some common symbols
(e.g. thrd_create) when built without support for C11 threads. This is usually not a problem,
however, expected return codes of thread functions differs from common glibc/musl implementation.

As the JSON plugin and librdkafka library are loaded after collector start, the common
version of symbols are used and the redefined ones are ignored. Later when librdkafka tries to
create its internal processing threads, it fails as thrd_create() returned unexpected return code.
This issue can be resolved by using RTLD_DEEPBIND flag of dlopen() function when loading the
plugin. However, this flag is a GNU extension, which is not supported by all C libraries, and
it can also break some other standard functions (e.g. C++ iostream).

Affected versions of the library are available in official package repositories of multiple
distributions. Therefore, we created this special version of JSON output which instructs the
collector core to use the RTLD_DEEPBIND flag.

This issue should be fixes since librdkafka v1.4.2. For more information see librdkafka
issue #2681 on Github.