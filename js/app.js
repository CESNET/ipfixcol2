// Obtain the root element
const rootAppElement = document.getElementById("configurator_app");
const colors = ["blue", "orange", "red"];
const columnNames = ["Input plugins", "Intermediate plugins", "Output plugins"];
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
const defaultConfig = {
    ipfixcol2: {
        inputPlugins: {
            input: []
        },
        intermediatePlugins: {
            intermediate: []
        },
        outputPlugins: {
            output: []
        }
    }
};
const inputModulesAvailable = [
    {
        name: "UDP input",
        plugin: "udp",
        params: {
            localPort: 4739,
            localIPAddress: "127.0.0.1",
            connectionTimeout: null,
            templateLifeTime: null,
            optionsTemplateLifeTime: null
        }
    },
    {
        name: "TCP input",
        plugin: "tcp",
        params: {
            localPort: 4739,
            localIPAddress: "127.0.0.1"
        }
    }
];
const intermediateModulesAvailable = [
    {
        name: "Flow anonymization",
        plugin: "anonymization",
        params: {
            type: ["CryptoPAn", "Truncation"],
            key: ""
        }
    }
];
const outputModulesAvailable = [
    {
        name: "JSON output",
        plugin: "json",
        params: {
            tcpFlags: ["formatted", "raw"],
            timestamp: ["formatted", "unix"],
            protocol: ["formatted", "raw"],
            ignoreUnknown: [true, false],
            ignoreOptions: [true, false],
            nonPrintableChar: [true, false],
            outputs: {
                server: [{ name: "", port: 0, blocking: [true, false] }],
                send: [
                    {
                        name: "",
                        ip: "",
                        port: 0,
                        protocol: ["tcp", "udp"],
                        blocking: [true, false]
                    }
                ],
                file: [
                    {
                        name: "",
                        path: "",
                        prefix: "",
                        timeWindow: 300,
                        timeAlignment: ["yes", "no"]
                    }
                ],
                print: { name: "" }
            }
        }
    },
    {
        name: "Dummy output",
        plugin: "dummy",
        params: {
            delay: 0
        }
    },
    {
        name: "LNF storage",
        plugin: "lnfstore",
        params: {
            storagePath: "",
            compress: ["no", "yes"],
            identificatorField: "",
            dumpInterval: {
                timeWindow: 300,
                align: ["yes", "no"]
            },
            index: {
                enable: ["no", "yes"],
                autosize: ["yes", "no"],
                estimatedItemCount: 100000,
                falsePositiveProbability: 0.01
            }
        }
    },
    {
        name: "UniRec plugin",
        plugin: "unirec",
        params: {
            uniRecFormat: "",
            trapIfcCommon: {
                timeout: ["HALF_WAIT", "WAIT", "NO_WAIT"],
                buffer: [true, false],
                autoflush: 500000
            },
            trapIfcSpec: [
                {
                    unix: {
                        name: "",
                        maxClients: 64
                    }
                },
                {
                    tcp: {
                        port: 0,
                        maxClients: 64
                    }
                },
                {
                    "tcp-tls": {
                        port: 0,
                        maxClients: 64,
                        keyFile: "",
                        certFile: "",
                        caFile: ""
                    }
                },
                {
                    file: {
                        name: "",
                        mode: ["write", "append"],
                        time: 0,
                        size: 0
                    }
                }
            ]
        }
    }
];
const modulesAvailable = [
    inputModulesAvailable,
    intermediateModulesAvailable,
    outputModulesAvailable
];
const x2js = new X2JS();

class App extends React.Component {
    constructor(props) {
        super(props);
    }

    render() {
        return <Form />;
    }
}

class Form extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            modules: [[], [], []]
        };

        // this.state.modules[0].push(modulesAvailable[0][1]);
        // this.state.modules[0].push(modulesAvailable[0][0]);
        // this.state.modules[2].push(modulesAvailable[2][0]);
        // this.state.modules[1].push(modulesAvailable[1][0]);
    }

    addModule = (columnName, index) => {
        this.setState(state => {
            var newModules;
            switch (columnName) {
                case columnNames[0]:
                    newModules = this.state.modules[0].concat(modulesAvailable[0][index]);
                    return {
                        modules: [newModules, state.modules[1], state.modules[2]]
                    };
                case columnNames[1]:
                    newModules = this.state.modules[1].concat(modulesAvailable[1][index]);
                    return {
                        modules: [state.modules[0], newModules, state.modules[2]]
                    };
                case columnNames[2]:
                    newModules = this.state.modules[2].concat(modulesAvailable[2][index]);
                    return {
                        modules: [state.modules[0], state.modules[1], newModules]
                    };
                default:
                    console.log("error while adding module");
            }
        });
    };

    removeModule = (columnName, index) => {
        this.setState(state => {
            var newModules;
            switch (columnName) {
                case columnNames[0]:
                    newModules = this.state.modules[0].filter((_, j) => index !== j);
                    return {
                        modules: [newModules, state.modules[1], state.modules[2]]
                    };
                case columnNames[1]:
                    newModules = this.state.modules[1].filter((_, j) => index !== j);
                    return {
                        modules: [state.modules[0], newModules, state.modules[2]]
                    };
                case columnNames[2]:
                    newModules = this.state.modules[2].filter((_, j) => index !== j);
                    return {
                        modules: [state.modules[0], state.modules[1], newModules]
                    };
                default:
                    console.log("error while deleting module");
            }
        });
    };

    renderXML() {
        var config = defaultConfig;
        config.ipfixcol2.inputPlugins.input = this.state.modules[0];
        config.ipfixcol2.intermediatePlugins.intermediate = this.state.modules[1];
        config.ipfixcol2.outputPlugins.output = this.state.modules[2];
        var xml = x2js.json2xml_str(config);
        return <textarea value={formatXml(xml)} readOnly />;
    }

    render() {
        return (
            <div className="form">
                <FormColumn
                    key={columnNames[0]}
                    parent={this}
                    modules={this.state.modules[0]}
                    color={colors[0]}
                    name={columnNames[0]}
                    modulesAvailable={modulesAvailable[0]}
                    addModule={this.addModule}
                />
                <FormColumn
                    key={columnNames[1]}
                    parent={this}
                    modules={this.state.modules[1]}
                    color={colors[1]}
                    name={columnNames[1]}
                    modulesAvailable={modulesAvailable[1]}
                    addModule={this.addModule}
                />
                <FormColumn
                    key={columnNames[2]}
                    parent={this}
                    modules={this.state.modules[2]}
                    color={colors[2]}
                    name={columnNames[2]}
                    modulesAvailable={modulesAvailable[2]}
                    addModule={this.addModule}
                />
                <EditModule JSONschema={jsonSchemaUDP} />
                {this.renderXML()}
            </div>
        );
    }
}

class FormColumn extends React.Component {
    constructor(props) {
        super(props);
    }

    addModule(name, index) {
        this.props.parent.addModule(name, index);
        console.log("plugin added");
    }

    removeModule(name, index) {
        this.props.parent.removeModule(name, index);
        console.log("plugin removed");
    }

    render() {
        return (
            <div className={"column " + this.props.color}>
                <h2>{this.props.name}</h2>
                {this.props.modules.map((module, index) => {
                    return (
                        <Module
                            key={index}
                            module={module}
                            onRemove={() => this.removeModule(this.props.name, index)}
                        />
                    );
                })}
                <div className={"addModule"} id={"add" + this.props.name}>
                    <button>Add plugin</button>
                    <div className={"modules"}>
                        {this.props.modulesAvailable.map((moduleAvailable, index) => {
                            return (
                                <ModuleAvailable
                                    key={index}
                                    module={moduleAvailable}
                                    onClick={() => this.addModule(this.props.name, index)}
                                />
                            );
                        })}
                    </div>
                </div>
            </div>
        );
    }
}

function ModuleAvailable(props) {
    return <button onClick={props.onClick}>{props.module.plugin}</button>;
}

class Module extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            detailVisible: false
        };
    }

    setDetailVisibile = () => {
        this.setState(state => {
            return { detailVisible: true };
        });
        console.log("set visible");
    };

    setDetailHidden = () => {
        this.setState(state => {
            return { detailVisible: false };
        });
        console.log("set hidden");
    };

    render() {
        return (
            <div className={"module" + (this.state.detailVisible ? " visible" : "")}>
                <div className={"header"}>
                    <button
                        onClick={
                            this.state.detailVisible
                                ? () => this.setDetailHidden()
                                : () => this.setDetailVisibile()
                        }
                    >
                        <i className={"fas fa-angle-down"}></i>
                    </button>
                    <h3>{this.props.module.name}</h3>
                    <button onClick={this.props.onRemove}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>
                <div className={"content"}>
                    <p>plugin: {this.props.module.plugin}</p>
                    <p>params: {this.props.module.params.toString()}</p>
                </div>
            </div>
        );
    }
}

class EditModule extends React.Component {
    render() {
        return (
            <Properties objectProperties={this.props.JSONschema} required={true} isRoot={true} />
        );
    }
}

function OptionalProperty(props) {
    return <button>{props.name}</button>;
}

class Properties extends React.Component {
    render() {
        var className = this.props.isRoot ? "rootProps" : "innerProps";
        var name = this.props.isRoot ? "" : this.props.name;
        var optional = "";
        if (
            Object.keys(this.props.objectProperties.properties).length >
            this.props.objectProperties.required.length
        ) {
            optional = (
                <div className={"addOptional"}>
                    <button>Add optional parameter</button>
                    <div className={"parameters"}>
                        {Object.keys(this.props.objectProperties.properties).map(propertyName => {
                            if (!this.props.objectProperties.required.includes(propertyName)) {
                                return <OptionalProperty key={propertyName} name={propertyName} />;
                            }
                        })}
                    </div>
                </div>
            );
        }
        return (
            <div className={className}>
                <p>{name}</p>
                {Object.keys(this.props.objectProperties.properties).map(propertyName => {
                    if (!this.props.objectProperties.required.includes(propertyName)) {
                        return;
                    }
                    switch (this.props.objectProperties.properties[propertyName].type) {
                        case "string":
                            return (
                                <StringProperty
                                    key={propertyName}
                                    name={propertyName}
                                    required={true}
                                    stringProperties={
                                        this.props.objectProperties.properties[propertyName]
                                    }
                                />
                            );
                        case "integer":
                            return (
                                <IntegerProperty
                                    key={propertyName}
                                    name={propertyName}
                                    required={true}
                                    indegerProperties={
                                        this.props.objectProperties.properties[propertyName]
                                    }
                                />
                            );
                        case "object":
                            return (
                                <Properties
                                    key={propertyName}
                                    name={propertyName}
                                    required={true}
                                    isRoot={false}
                                    objectProperties={
                                        this.props.objectProperties.properties[propertyName]
                                    }
                                />
                            );
                    }
                })}
                {optional}
            </div>
        );
    }
}

class StringProperty extends React.Component {
    render() {
        var value = "";
        var readOnly = false;
        if (this.props.stringProperties.hasOwnProperty("default")) {
            value = this.props.stringProperties.default;
        }
        if (this.props.stringProperties.hasOwnProperty("const")) {
            value = this.props.stringProperties.const;
            readOnly = true;
        }
        return (
            <div>
                <label>{this.props.name}</label>
                <input
                    type={"text"}
                    name={this.props.name}
                    value={value}
                    readOnly={readOnly}
                    required={this.props.required}
                />
            </div>
        );
    }
}

class IntegerProperty extends React.Component {
    render() {
        var value = "";
        var readOnly = false;
        var min = null;
        var max = null;
        if (this.props.indegerProperties.hasOwnProperty("default")) {
            value = this.props.indegerProperties.default;
        }
        if (this.props.indegerProperties.hasOwnProperty("const")) {
            value = this.props.indegerProperties.const;
            readOnly = true;
        }
        if (this.props.indegerProperties.hasOwnProperty("minimum")) {
            min = this.props.indegerProperties.minimum;
        }
        if (this.props.indegerProperties.hasOwnProperty("maximum")) {
            max = this.props.indegerProperties.maximum;
        }
        return (
            <div>
                <label>{this.props.name}</label>
                <input
                    type={"number"}
                    name={this.props.name}
                    step={1}
                    value={value}
                    readOnly={readOnly}
                    required
                    min={min}
                    max={max}
                />
            </div>
        );
    }
}

// ========================================

ReactDOM.render(<App />, rootAppElement);
