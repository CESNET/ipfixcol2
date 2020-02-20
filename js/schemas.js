const jsonSchemaUDP = {
    $schema: "http://json-schema.org/draft-07/schema#",
    title: "UDP input",
    description: "UDP input plugin",
    type: "object",
    properties: {
        name: {
            title: "Name",
            description: "User identification of the instance. Must be unique within the section.",
            type: "string",
            default: "UDP input",
            minLength: 1
        },
        plugin: {
            title: "Plugin",
            description:
                "Internal identification of a plugin to whom the instance belongs. The identification is assigned by an author of the plugin.",
            type: "string",
            const: "udp"
        },
        params: {
            title: "Parameters",
            description: "Configuration parameters of the instance.",
            type: "object",
            properties: {
                localPort: {
                    title: "Local port",
                    description: "Local port on which the plugin listens. [default: 4739]",
                    type: "integer",
                    minimum: 0,
                    default: 4739
                },
                localIPAddress: {
                    title: "Local IP address",
                    description:
                        "Local IPv4/IPv6 address on which the UDP input plugin listens. If the element is left empty, the plugin binds to all available network interfaces. The element can occur multiple times (one IP address per occurrence) to manually select multiple interfaces. [default: empty]",
                    type: "string",
                    default: ""
                },
                connectionTimeout: {
                    title: "Connection timeout",
                    description:
                        "Exporter connection timeout in seconds. If no message is received from an exporter for a specified time, the connection is considered closed and all resources (associated with the exporter) are removed, such as flow templates, etc. [default: 600]",
                    type: "integer",
                    default: 600
                },
                templateLifeTime: {
                    title: "Template lifetime",
                    description:
                        "(Options) Template lifetime in seconds for all UDP Transport Sessions terminating at this UDP socket. (Options) Templates that are not received again within the configured lifetime become invalid. The lifetime of Templates and Options Templates should be at least three times higher than the same values configured on the corresponding exporter. [default: 1800]",
                    type: "integer",
                    default: 1800
                },
                optionsTemplateLifeTime: {
                    title: "Options template lifetime",
                    description:
                        "(Options) Template lifetime in seconds for all UDP Transport Sessions terminating at this UDP socket. (Options) Templates that are not received again within the configured lifetime become invalid. The lifetime of Templates and Options Templates should be at least three times higher than the same values configured on the corresponding exporter. [default: 1800]",
                    type: "integer",
                    default: 1800
                }
            },
            required: ["localPort", "localIPAddress"]
        }
    },
    required: ["name", "plugin", "params"]
};
const jsonSchemaTCP = {
    $schema: "http://json-schema.org/draft-07/schema#",
    title: "TCP input",
    description: "TCP input plugin",
    type: "object",
    properties: {
        name: {
            title: "Name",
            description: "User identification of the instance. Must be unique within the section.",
            type: "string",
            default: "TCP input",
            minLength: 1
        },
        plugin: {
            title: "Plugin",
            description:
                "Internal identification of a plugin to whom the instance belongs. The identification is assigned by an author of the plugin.",
            type: "string",
            const: "tcp"
        },
        params: {
            title: "Parameters",
            description: "Configuration parameters of the instance.",
            type: "object",
            properties: {
                localPort: {
                    title: "Local port",
                    description: "Local port on which the plugin listens. [default: 4739]",
                    type: "integer",
                    minimum: 0,
                    default: 4739
                },
                localIPAddress: {
                    title: "Local IP address",
                    description:
                        "Local IPv4/IPv6 address on which the TCP input plugin listens. If the element is left empty, the plugin binds to all available network interfaces. The element can occur multiple times (one IP address per occurrence) to manually select multiple interfaces. [default: empty]",
                    type: "string",
                    default: ""
                }
            },
            required: ["localPort", "localIPAddress"]
        }
    },
    required: ["name", "plugin", "params"]
};
const jsonSchemaAnonymization = {
    $schema: "http://json-schema.org/draft-07/schema#",
    title: "Flow anonymization",
    description: "Flow anonymization plugin",
    type: "object",
    properties: {
        name: {
            title: "Name",
            description: "User identification of the instance. Must be unique within the section.",
            type: "string",
            default: "Flow anonymization",
            minLength: 1
        },
        plugin: {
            title: "Plugin",
            description:
                "Internal identification of a plugin to whom the instance belongs. The identification is assigned by an author of the plugin.",
            type: "string",
            const: "anonymization"
        },
        params: {
            title: "Parameters",
            description: "Configuration parameters of the instance.",
            type: "object",
            properties: {
                type: {
                    title: "Type",
                    description:
                        'Type of anonymization method. The string is case insensitive.\nCryptoPAn: Cryptography-based sanitization and prefix-preserving method. The mapping from original IP addresses to anonymized IP addresses is one-to-one and if two original IP addresses share a k-bit prefix, their anonymized mappings will also share a k-bit prefix. Be aware that this cryptography method is very demanding and can limit throughput of the collector.\nTruncation: This method keeps the top part and erases the bottom part of an IP address. Compared to the CryptoPAn method, it is considerably faster, however, mapping from the original to anonymized IP address is many-to-one. For example the IPv4 address "1.2.3.4" is mapped to the address "1.2.0.0".',
                    type: "string",
                    enum: ["CryptoPAn", "Truncation"]
                },
                key: {
                    title: "Key",
                    description:
                        "Optional cryptography key for CryptoPAn anonymization. The length of the string must be exactly 32 bytes. If the key is not specified, a random one is generated during the initialization.",
                    type: "string",
                    default: ""
                }
            },
            required: ["type", "key"]
        }
    },
    required: ["name", "plugin", "params"]
};
const jsonSchemaJSON = {
    $schema: "http://json-schema.org/draft-07/schema#",
    title: "JSON output",
    description: "JSON output plugin",
    type: "object",
    properties: {
        name: {
            title: "Name",
            description: "User identification of the instance. Must be unique within the section.",
            type: "string",
            default: "JSON output",
            minLength: 1
        },
        plugin: {
            title: "Plugin",
            description:
                "Internal identification of a plugin to whom the instance belongs. The identification is assigned by an author of the plugin.",
            type: "string",
            const: "json"
        },
        params: {
            title: "Parameters",
            description: "Configuration parameters of the instance.",
            type: "object",
            properties: {
                tcpFlags: {
                    title: "TCP flags",
                    description:
                        'Convert TCP flags to common textual representation (formatted, e.g. ".A..S.") or to a number (raw). [values: formatted/raw, default: formatted]',
                    type: "string",
                    default: "formatted",
                    enum: ["formatted", "raw"]
                },
                timestamp: {
                    title: "Timestamp",
                    description:
                        'Convert timestamp to ISO 8601 textual representation (all timestamps in UTC and milliseconds, e.g. "2018-01-22T09:29:57.828Z") or to a unix timestamp (all timestamps in milliseconds). [values: formatted/unix, default: formatted]',
                    type: "string",
                    default: "formatted",
                    enum: ["formatted", "unix"]
                },
                protocol: {
                    title: "Protocol",
                    description:
                        'Convert protocol identification to formatted style (e.g. instead 6 writes "TCP") or to a number. [values: formatted/raw, default: formatted]',
                    type: "string",
                    default: "formatted",
                    enum: ["formatted", "raw"]
                },
                ignoreUnknown: {
                    title: "Ignore unknown",
                    description:
                        "Skip unknown Information Elements (i.e. record fields with unknown name and data type). If disabled, data of unknown elements are formatted as unsigned integer or hexadecimal values. For more information, see octetArrayAsUint option. [values: true/false, default: true]",
                    type: "boolean",
                    default: true,
                    enum: [true, false]
                },
                ignoreOptions: {
                    title: "Ignore options",
                    description:
                        "Skip non-flow records used for reporting metadata about IPFIX Exporting and Metering Processes (i.e. records described by Options Templates). [values: true/false, default: true]",
                    type: "boolean",
                    default: true,
                    enum: [true, false]
                },
                nonPrintableChar: {
                    title: "Non-printable characters",
                    description:
                        "Ignore non-printable characters (newline, tab, control characters, etc.) in IPFIX strings. If disabled, these characters are escaped on output. [values: true/false, default: true]",
                    type: "boolean",
                    default: true,
                    enum: [true, false]
                },
                octetArrayAsUint: {
                    title: "Octet array as uint",
                    description:
                        'Converter each IPFIX field with octetArray type (including IPFIX fields with unknown definitions) as unsigned integer if the size of the field is less or equal to 8 bytes. Fields with the size above the limit are always converted as string representing hexadecimal value, which is typically in network byte order (e.g. "0x71E1"). Keep on mind, that there might be octetArray fields with variable length that might be interpreted differently based on their size. If disabled, octetArray fields are never interpreted as unsigned integers. [values: true/false, default: true]',
                    type: "boolean",
                    default: true,
                    enum: [true, false]
                },
                numericNames: {
                    title: "Numeric names",
                    description:
                        'Use only short identification of Information Elements (i.e. "enXX:idYY"). If enabled, the short version is used even if the definition of the field is known. This option can help to create a shorter JSON records with key identifiers which are independent on the internal configuration. [values: true/false, default: false]',
                    type: "boolean",
                    default: false,
                    enum: [true, false]
                },
                splitBiflow: {
                    title: "Split Biflow",
                    description:
                        "In case of Biflow records, split the record to two unidirectional flow records. Non-biflow records are unaffected. [values: true/false, default: false]",
                    type: "boolean",
                    default: false,
                    enum: [true, false]
                },
                detailedInfo: {
                    title: "Detailed info",
                    description:
                        'Add additional information about the IPFIX message (such as export time, Observation Domain ID, IP address of the exporter, etc.) to which each record belongs. Additional fields starts with "ipfix:" prefix. [values: true/false, default: false]',
                    type: "boolean",
                    default: false,
                    enum: [true, false]
                },
                templateInfo: {
                    title: "Template info",
                    description:
                        "Convert Template and Options Template records. See the particular section below for information about the formatting of these records. [values: true/false, default: false]",
                    type: "boolean",
                    default: false,
                    enum: [true, false]
                },
                outputs: {
                    title: "Output types",
                    description:
                        "At least one output must be configured. Multiple server/send/file outputs can be used at the same time if the outputs are not in collision with each other.",
                    type: "object",
                    properties: {
                        server: {
                            title: "Server",
                            type: "array",
                            minItems: 1,
                            items: {
                                title: "Server",
                                description:
                                    'TCP (push) server provides data on a local port. Converted records are automatically send to all clients that are connected to the port. To test the server you can use, for example, ncat(1) utility: "ncat <server ip> <port>".',
                                type: "object",
                                properties: {
                                    name: {
                                        title: "Name",
                                        description:
                                            "Identification name of the output. Used only for readability.",
                                        type: "string",
                                        minLength: 1
                                    },
                                    port: {
                                        title: "Port",
                                        description: "Local port number of the server.",
                                        type: "integer"
                                    },
                                    blocking: {
                                        title: "Blocking",
                                        description:
                                            "Enable blocking on TCP sockets (true/false). If blocking mode is disabled and a client is not able to retrieve records fast enough, some flow records may be dropped (only individual clients are affected). On the other hand, if the blocking mode is enabled, no records are dropped. However, if at least one client is slow, the plugin waits (i.e. blocks) until data are send. This can significantly slow down the whole collector and other output plugins because processing records is suspended. In the worst-case scenario, if the client is not responding at all, the whole collector is blocked! Therefore, it is usually preferred (and much safer) to disable blocking.",
                                        type: "boolean",
                                        enum: [true, false]
                                    }
                                },
                                required: ["name", "port", "blocking"]
                            }
                        },
                        send: {
                            title: "Send",
                            type: "array",
                            minItems: 1,
                            items: {
                                title: "Send",
                                description:
                                    'Send records over network to a client. If the destination is not reachable or the client is disconnected, the plugin drops all records and tries to reconnect every 5 seconds. As with the server, you can verify functionality using ncat(1) utility: "ncat -lk <local ip> <local port>"',
                                type: "object",
                                properties: {
                                    name: {
                                        title: "Name",
                                        description:
                                            "Identification name of the output. Used only for readability.",
                                        type: "string",
                                        minLength: 1
                                    },
                                    ip: {
                                        title: "IP address",
                                        description: "IPv4/IPv6 address of the client",
                                        type: "string",
                                        minLength: 1
                                    },
                                    port: {
                                        title: "Port",
                                        description: "Remote port number",
                                        type: "integer"
                                    },
                                    protocol: {
                                        title: "Protocol",
                                        description:
                                            "Transport protocol: TCP or UDP (this field is case insensitive)",
                                        type: "string",
                                        enum: ["TCP", "UDP"]
                                    },
                                    blocking: {
                                        title: "Blocking",
                                        description:
                                            "Enable blocking on TCP sockets (true/false). If blocking mode is disabled and a client is not able to retrieve records fast enough, some flow records may be dropped (only individual clients are affected). On the other hand, if the blocking mode is enabled, no records are dropped. However, if at least one client is slow, the plugin waits (i.e. blocks) until data are send. This can significantly slow down the whole collector and other output plugins because processing records is suspended. In the worst-case scenario, if the client is not responding at all, the whole collector is blocked! Therefore, it is usually preferred (and much safer) to disable blocking.",
                                        type: "boolean",
                                        enum: [true, false]
                                    }
                                },
                                required: ["name", "ip", "port", "protocol", "blocking"]
                            }
                        },
                        file: {
                            title: "File",
                            type: "array",
                            minItems: 1,
                            items: {
                                title: "File",
                                description: "Store data to files.",
                                type: "object",
                                properties: {
                                    name: {
                                        title: "Name",
                                        description:
                                            "Identification name of the output. Used only for readability.",
                                        type: "string",
                                        minLength: 1
                                    },
                                    path: {
                                        title: "Path",
                                        description:
                                            'The path specifies storage directory for data collected by the plugin. Format specifiers for day, month, etc. are supported so you can create suitable directory hierarchy. See "strftime" for conversion specification. (Note: UTC time)',
                                        type: "string",
                                        minLength: 1
                                    },
                                    prefix: {
                                        title: "Prefix",
                                        description: "Specifies name prefix for output files.",
                                        type: "string",
                                        minLength: 1
                                    },
                                    timeWindow: {
                                        title: "Time window",
                                        description:
                                            "Specifies the time interval in seconds to rotate files [minimum 60, default 300]",
                                        type: "integer",
                                        default: 300,
                                        minimum: 60
                                    },
                                    timeAlignment: {
                                        title: "Time alignment",
                                        description:
                                            "Align file rotation with next N minute interval (yes/no).",
                                        type: "string",
                                        enum: ["yes", "no"]
                                    },
                                    compression: {
                                        title: "Compression",
                                        description:
                                            "Data compression helps to significantly reduce size of output files. Following compression algorithms are available:\nnone: Compression disabled [default]\ngzip: GZIP compression",
                                        type: "string",
                                        enum: ["none", "gzip"],
                                        default: "none"
                                    }
                                },
                                required: [
                                    "name",
                                    "path",
                                    "prefix",
                                    "timeWindow",
                                    "timeAlignment",
                                    "compression"
                                ]
                            }
                        },
                        print: {
                            title: "Print",
                            description: "Write data on standard output.",
                            type: "object",
                            properties: {
                                name: {
                                    title: "Name",
                                    description:
                                        "Identification name of the output. Used only for readability.",
                                    type: "string",
                                    minLength: 1
                                }
                            },
                            required: ["name"]
                        }
                    },
                    minProperties: 1
                }
            },
            required: [
                "tcpFlags",
                "timestamp",
                "protocol",
                "ignoreUnknown",
                "ignoreOptions",
                "nonPrintableChar",
                "octetArrayAsUint",
                "numericNames",
                "splitBiflow",
                "detailedInfo",
                "templateInfo",
                "outputs"
            ]
        }
    },
    required: ["name", "plugin", "params"]
};
const jsonSchemaDummy = {
    $schema: "http://json-schema.org/draft-07/schema#",
    title: "Dummy output",
    description: "Dummy output plugin",
    type: "object",
    properties: {
        name: {
            title: "Name",
            description: "User identification of the instance. Must be unique within the section.",
            type: "string",
            default: "Dummy output",
            minLength: 1
        },
        plugin: {
            title: "Plugin",
            description:
                "Internal identification of a plugin to whom the instance belongs. The identification is assigned by an author of the plugin.",
            type: "string",
            const: "dummy"
        },
        params: {
            title: "Parameters",
            description: "Configuration parameters of the instance.",
            type: "object",
            properties: {
                delay: {
                    title: "Delay",
                    description:
                        "Minimum delay between processing of two consecutive messages in microseconds.",
                    type: "integer",
                    minimum: 0
                }
            },
            required: ["delay"]
        }
    },
    required: ["name", "plugin", "params"]
};
const jsonSchemaLNF = {
    $schema: "http://json-schema.org/draft-07/schema#",
    title: "LNF storage",
    description: "LNF storage plugin",
    type: "object",
    properties: {
        name: {
            title: "Name",
            description: "User identification of the instance. Must be unique within the section.",
            type: "string",
            default: "LNF storage",
            minLength: 1
        },
        plugin: {
            title: "Plugin",
            description:
                "Internal identification of a plugin to whom the instance belongs. The identification is assigned by an author of the plugin.",
            type: "string",
            const: "lnfstore"
        },
        params: {
            title: "Parameters",
            description: "Configuration parameters of the instance.",
            type: "object",
            properties: {
                storagePath: {
                    title: "Storage path",
                    description:
                        "The path element specifies the storage directory for data files. Keep on mind that the path must exist in your system. Otherwise, no records are stored. All files will be stored based on the configuration using the following template: <storagePath>/YYYY/MM/DD/lnf.<suffix> where YYYY/MM/DD means year/month/day and <suffix> represents a UTC timestamp in format YYMMDDhhmmss.",
                    type: "string",
                    minLength: 1
                },
                compress: {
                    title: "Compress",
                    description:
                        "Enable/disable LZO compression for files. [values: yes/no, default: no]",
                    type: "string",
                    default: "no",
                    enum: ["yes", "no"]
                },
                identificatorField: {
                    title: "Identificator field",
                    description:
                        "Specifies an identification string, which is put into statistic records to describe the source. [default: <empty>]",
                    type: "string",
                    default: "",
                    minLength: 1
                },
                dumpInterval: {
                    title: "Dump interval",
                    description: "Configuration of output files rotation.",
                    type: "object",
                    properties: {
                        timeWindow: {
                            title: "Time window",
                            description:
                                "Specifies time interval in seconds to rotate files i.e. close the current file and create a new one. [default: 300]",
                            type: "integer",
                            default: 300,
                            minimum: 0
                        },
                        align: {
                            title: "Align",
                            description:
                                "Align file rotation with next N minute interval. For example, if enabled and window size is 5 minutes long, files will be created at 0, 5, 10, etc. [values: yes/no, default: yes]",
                            type: "string",
                            default: "yes",
                            enum: ["yes", "no"]
                        }
                    },
                    required: ["timeWindow", "align"]
                },
                index: {
                    title: "Index",
                    description:
                        'Configuration of IP address indexes. Index files are independent and exists besides "lnf.*" files as "bfi.*" files with matching identification.',
                    type: "object",
                    properties: {
                        enable: {
                            title: "Enable",
                            description:
                                "Enable/disable Bloom Filter indexes. [values: yes/no, default: no]",
                            type: "string",
                            default: "no",
                            enum: ["yes", "no"]
                        },
                        autosize: {
                            title: "Autosize",
                            description:
                                "Enable/disable automatic resize of index files based on the number of unique IP addresses in the last dump interval. [values: yes/no, default: yes]",
                            type: "string",
                            default: "yes",
                            enum: ["yes", "no"]
                        },
                        estimatedItemCount: {
                            title: "Estimated item count",
                            description:
                                "Expected number of unique IP addresses in dump interval. If autosize is enabled this value is continuously recalculated to suit current utilization. The value affects the size of index files i.e. higher value, larger files. [default: 100000]",
                            type: "integer",
                            default: 100000,
                            minimum: 0
                        },
                        falsePositiveProbability: {
                            title: "False positive probability",
                            description:
                                "False positive probability of the index. The probability that presence test of an IP address indicates that the IP address is present in a data file, when it actually is not. It does not affect the situation when the IP address is actually in the data file i.e. if the IP is in the file, the result of the test is always correct. The value affects the size of index files i.e. smaller value, larger files. [default: 0.01]",
                            type: "number",
                            default: 0.01,
                            minimum: 0
                        }
                    },
                    required: ["enable", "autosize"]
                }
            },
            required: ["storagePath", "compress", "dumpInterval", "index"]
        }
    },
    required: ["name", "plugin", "params"]
};
const jsonSchemaUniRec = {
    $schema: "http://json-schema.org/draft-07/schema#",
    title: "UniRec plugin",
    description: "UniRec plugin",
    type: "object",
    properties: {
        name: {
            title: "Name",
            description: "User identification of the instance. Must be unique within the section.",
            type: "string",
            default: "UniRec plugin",
            minLength: 1
        },
        plugin: {
            title: "Plugin",
            description:
                "Internal identification of a plugin to whom the instance belongs. The identification is assigned by an author of the plugin.",
            type: "string",
            const: "unirec"
        },
        params: {
            title: "Parameters",
            description: "Configuration parameters of the instance.",
            type: "object",
            properties: {
                uniRecFormat: {
                    title: "UniRec format",
                    description:
                        "Comma separated list of UniRec fields. All fields are mandatory be default, therefore, if a flow record to convert doesn't contain all mandatory fields, it is dropped. However, UniRec fields that start with '?' are optional and if they are not present in the record (e.g. TCP_FLAGS) default value (typically zero) is used. List of all supported UniRec fields is defined in unirec-element.txt file. Example value: \"DST_IP,SRC_IP,BYTES,DST_PORT,?TCP_FLAGS,SRC_PORT,PROTOCOL\".",
                    type: "string",
                    minLength: 1
                },
                trapIfcCommon: {
                    title: "TRAP interface common",
                    description:
                        "The following parameters can be used with any type of a TRAP interface. There are parameters of the interface that are normally let default. However, it is possible to override them by user.",
                    type: "object",
                    properties: {
                        timeout: {
                            title: "Timeout",
                            description:
                                'Time in microseconds that the output interface can block waiting for message to be send. There are also special values: "WAIT" (block indefinitely), "NO_WAIT" (don\'t block), "HALF_WAIT" (block only if some client is connected). Be very careful, inappropriate configuration can significantly slowdown the collector and lead to loss of data. [default: "HALF_WAIT"]',
                            type: "string",
                            default: "HALF_WAIT",
                            enum: ["WAIT", "HALF_WAIT", "NO_WAIT"]
                        },
                        buffer: {
                            title: "Buffer",
                            description:
                                "Enable buffering of data and sending in larger bulks (increases throughput) [default: true]",
                            type: "boolean",
                            default: true,
                            enum: [true, false]
                        },
                        autoflush: {
                            title: "Automatic flush",
                            description:
                                "Automatically flush data even if the output buffer is not full every X microseconds. If the automatic flush is disabled (value 0), data are not send until the buffer is full. [default: 500000]",
                            type: "integer",
                            default: 500000
                        }
                    },
                    required: ["timeout", "buffer", "autoflush"]
                },
                trapIfcSpec: {
                    title: "TRAP interface specification",
                    description: "Specification of interface type and its parameters.",
                    type: "object",
                    properties: {
                        unix: {
                            title: "UNIX",
                            description:
                                "Communicates through a UNIX socket. The output interface creates a socket and listens, input interface connects to it. There may be more than one input interfaces connected to the output interface, every input interface will get the same data.",
                            type: "object",
                            properties: {
                                name: {
                                    title: "Name",
                                    description:
                                        "Socket name i.e. any string usable as a file name. The name MUST not include colon character.",
                                    type: "string",
                                    minLength: 1
                                },
                                maxClients: {
                                    title: "Max clients",
                                    description:
                                        "Maximal number of connected clients (input interfaces). [default: 64]",
                                    type: "integer",
                                    default: 64
                                }
                            },
                            required: ["name", "maxClients"]
                        },
                        tcp: {
                            title: "TCP",
                            description:
                                "Communicates through a TCP socket. The output interface listens on a given port, input interface connects to it. There may be more than one input interfaces connected to the output interface, every input interface will get the same data.",
                            type: "object",
                            properties: {
                                port: {
                                    title: "Port",
                                    description: "Local port number",
                                    type: "integer",
                                    minimum: 0
                                },
                                maxClients: {
                                    title: "Max clients",
                                    description:
                                        "Maximal number of connected clients (input interfaces). [default: 64]",
                                    type: "integer",
                                    default: 64
                                }
                            },
                            required: ["port", "maxClients"]
                        },
                        "tcp-tls": {
                            title: "TCP-TLS",
                            description:
                                "Communicates through a TCP socket after establishing encrypted connection. You have to provide a certificate, a private key and a CA chain file with trusted CAs. Otherwise, same as TCP: The output interface listens on a given port, input interface connects to it. There may be more than one input interfaces connected to the output interface, every input interface will get the same data. Paths to files MUST not include colon character.",
                            type: "object",
                            properties: {
                                port: {
                                    title: "Port",
                                    description: "Local port number",
                                    type: "integer",
                                    minimum: 0
                                },
                                maxClients: {
                                    title: "Max clients",
                                    description:
                                        "Maximal number of connected clients (input interfaces). [default: 64]",
                                    type: "integer",
                                    default: 64
                                },
                                keyFile: {
                                    title: "Key file",
                                    description: "Path to a file of a private key in PEM format.",
                                    type: "string",
                                    minLength: 1
                                },
                                certFile: {
                                    title: "Certificate file",
                                    description:
                                        "Path to a file of certificate chain in PEM format.",
                                    type: "string",
                                    minLength: 1
                                },
                                caFile: {
                                    title: "CA file",
                                    description:
                                        "Path to a file of trusted CA certificates in PEM format.",
                                    type: "string",
                                    minLength: 1
                                }
                            },
                            required: ["port", "maxClients", "keyFile", "certFile", "caFile"]
                        },
                        file: {
                            title: "File",
                            description:
                                "Stores UniRec records into a file. The interface allows to split data into multiple files after a specified time or a size of the file. If both options are enabled at the same time, the data are split primarily by time, and only if a file of one time interval exceeds the size limit, it is further split. The index of size-split file is appended after the time.",
                            type: "object",
                            properties: {
                                name: {
                                    title: "Name",
                                    description:
                                        "Name of the output file. The name MUST not include colon character.",
                                    type: "string",
                                    minLength: 1
                                },
                                mode: {
                                    title: "Mode",
                                    description:
                                        "Output mode: write/append. If the specified file exists, mode write overwrites it, mode append creates a new file with an integer suffix. [default: write]",
                                    type: "string",
                                    default: "write",
                                    enum: ["write", "append"]
                                },
                                time: {
                                    title: "Time",
                                    description:
                                        'If the parameter is non-zero, the output interface will split captured data to individual files as often, as value of this parameter (in minutes) indicates. The output interface creates unique file name for each file according to current timestamp in format: "filename.YYYYmmddHHMM". [default: 0]',
                                    type: "integer",
                                    default: 0
                                },
                                size: {
                                    title: "Size",
                                    description:
                                        "If the parameter is non-zero, the output interface will split captured data into individual files after a size of a current file (in MB) exceeds given threshold. Numeric suffix is added to the original file name for each file in ascending order starting with 0. [default: 0] ",
                                    type: "integer",
                                    default: 0
                                }
                            },
                            required: ["name", "mode", "time", "size"]
                        }
                    },
                    minProperties: 1,
                    maxProperties: 1
                }
            },
            required: ["uniRecFormat", "trapIfcCommon", "trapIfcSpec"]
        }
    },
    required: ["name", "plugin", "params"]
};
const jsonSchemaTimeCheck = {
    $schema: "http://json-schema.org/draft-07/schema#",
    title: "Time Check",
    description: "Time Check plugin",
    type: "object",
    properties: {
        name: {
            title: "Name",
            description: "User identification of the instance. Must be unique within the section.",
            type: "string",
            default: "TimeCheck output",
            minLength: 1
        },
        plugin: {
            title: "Plugin",
            description:
                "Internal identification of a plugin to whom the instance belongs. The identification is assigned by an author of the plugin.",
            type: "string",
            const: "timecheck"
        },
        params: {
            title: "Parameters",
            description: "Configuration parameters of the instance.",
            type: "object",
            properties: {
                devPast: {
                    title: "Deviation from the past",
                    description:
                        "Maximum allowed deviation between the current system time and timestamps from the past in seconds. The value must be greater than active and inactive timeouts of exporters and must also include a possible delay between expiration and processing on the collector. For example, let's say that active timeout and inactive timeout are 5 minutes and 30 seconds, respectively. Value 600 (i.e. 10 minutes) should be always enough for all flow data to be received and processed at the collector. [default: 600]",
                    type: "integer",
                    default: 600,
                    minimum: 0
                },
                devFuture: {
                    title: "Deviation from the future",
                    description:
                        "Maximum allowed deviation between the current time and timestamps from the future in seconds. The collector should never receive flows with timestamp from the future, therefore, the value should be usually set to 0. [default: 0]",
                    type: "integer",
                    default: 0,
                    minimum: 0
                }
            },
            required: ["devPast", "devFuture"]
        }
    },
    required: ["name", "plugin", "params"]
};
