const jsonSchemaUDP = {
    $schema: "http://json-schema.org/draft-07/schema#",
    title: "UDP input",
    desription: "UDP input plugin",
    type: "object",
    properties: {
        name: {
            type: "string",
            default: "UDP input"
        },
        plugin: {
            description: "plugin type identifier",
            type: "string",
            const: "udp"
        },
        props: {
            type: "object",
            properties: {
                localPort: {
                    type: "integer",
                    minimum: 0,
                    default: 4739
                },
                localIPAddress: {
                    type: "string",
                    default: ""
                },
                connectionTimeout: {
                    type: "integer",
                    default: 600
                },
                templateLifeTime: {
                    type: "integer",
                    default: 1800
                },
                optionsTemplateLifeTime: {
                    type: "integer",
                    default: 1800
                }
            },
            required: ["localPort", "localIPAddress"]
        }
    },
    required: ["name", "plugin", "props"]
};
const jsonSchemaTCP = {
    $schema: "http://json-schema.org/draft-07/schema#",
    title: "TCP input",
    desription: "TCP input plugin",
    type: "object",
    properties: {
        name: {
            type: "string",
            default: "TCP input"
        },
        plugin: {
            description: "plugin type identifier",
            type: "string",
            const: "tcp"
        },
        props: {
            type: "object",
            properties: {
                localPort: {
                    type: "integer",
                    minimum: 0,
                    default: 4739
                },
                localIPAddress: {
                    type: "string",
                    default: ""
                }
            },
            required: ["localPort", "localIPAddress"]
        }
    },
    required: ["name", "plugin", "props"]
};
const jsonSchemaAnonymization = {
    $schema: "http://json-schema.org/draft-07/schema#",
    title: "Flow anonymization",
    desription: "Flow anonymization plugin",
    type: "object",
    properties: {
        name: {
            type: "string",
            default: "Flow anonymization"
        },
        plugin: {
            description: "plugin type identifier",
            type: "string",
            const: "anonymization"
        },
        props: {
            type: "object",
            properties: {
                type: {
                    type: "string",
                    enum: ["CryptoPAn", "Truncation"]
                },
                key: {
                    type: "string",
                    default: ""
                }
            },
            required: ["type", "key"]
        }
    },
    required: ["name", "plugin", "props"]
};
const jsonSchemaJSON = {
    $schema: "http://json-schema.org/draft-07/schema#",
    title: "JSON output",
    desription: "JSON output plugin",
    type: "object",
    properties: {
        name: {
            type: "string",
            default: "JSON output"
        },
        plugin: {
            description: "plugin type identifier",
            type: "string",
            const: "json"
        },
        props: {
            type: "object",
            properties: {
                tcpFlags: {
                    type: "string",
                    default: "formatted",
                    enum: ["formatted", "raw"]
                },
                timestamp: {
                    type: "string",
                    default: "formatted",
                    enum: ["formatted", "unix"]
                },
                protocol: {
                    type: "string",
                    default: "formatted",
                    enum: ["formatted", "raw"]
                },
                ignoreUnknown: {
                    type: "boolean",
                    default: true,
                    enum: [true, false]
                },
                ignoreOptions: {
                    type: "boolean",
                    default: true,
                    enum: [true, false]
                },
                nonPrintableChar: {
                    type: "boolean",
                    default: true,
                    enum: [true, false]
                },
                outputs: {
                    type: "object",
                    properties: {
                        server: {
                            type: "array",
                            minItems: 1,
                            items: {
                                type: "object",
                                properties: {
                                    name: {
                                        type: "string"
                                    },
                                    port: {
                                        type: "integer"
                                    },
                                    blocking: {
                                        type: "boolean",
                                        enum: [true, false]
                                    }
                                },
                                required: ["name", "port", "blocking"]
                            }
                        },
                        send: {
                            type: "array",
                            minItems: 1,
                            items: {
                                type: "object",
                                properties: {
                                    name: {
                                        type: "string"
                                    },
                                    ip: {
                                        type: "string"
                                    },
                                    port: {
                                        type: "integer"
                                    },
                                    protocol: {
                                        type: "string",
                                        enum: ["TCP", "UDP"]
                                    },
                                    blocking: {
                                        type: "boolean",
                                        enum: [true, false]
                                    }
                                },
                                required: ["name", "ip", "port", "protocol", "blocking"]
                            }
                        },
                        file: {
                            type: "array",
                            minItems: 1,
                            items: {
                                type: "object",
                                properties: {
                                    name: {
                                        type: "string"
                                    },
                                    path: {
                                        type: "string"
                                    },
                                    prefix: {
                                        type: "string"
                                    },
                                    timeWindow: {
                                        type: "integer",
                                        default: 300,
                                        minimum: 60
                                    },
                                    timeAlignment: {
                                        type: "string",
                                        enum: ["yes", "no"]
                                    }
                                },
                                required: ["name", "path", "prefix", "timeWindow", "timeAlignment"]
                            }
                        },
                        print: {
                            type: "array",
                            minItems: 1,
                            maxItems: 1,
                            items: {
                                type: "object",
                                properties: {
                                    name: {
                                        type: "string"
                                    }
                                },
                                required: ["name"]
                            }
                        }
                    },
                    minProperties: 1
                }
            },
            required: ["tcpFlags", "timestamp", "protocol", "ignoreUnknown", "ignoreOptions", "nonPrintableChar", "outputs"]
        }
    },
    required: ["name", "plugin", "props"]
};
