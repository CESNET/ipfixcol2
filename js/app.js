const {
    Button,
    TextField,
    FormControl,
    FormHelperText,
    InputLabel,
    Select,
    Menu,
    MenuItem,
    Dialog,
    DialogTitle,
    DialogActions,
    DialogContent,
    Grid,
    TextareaAutosize
} = MaterialUI;
// Obtain the root element
const rootAppElement = document.getElementById("configurator_app");
const colors = ["blue", "orange", "red"];
const columnNames = ["Input plugins", "Intermediate plugins", "Output plugins"];
const moduleSchemas = [
    [jsonSchemaUDP, jsonSchemaTCP],
    [jsonSchemaAnonymization],
    [jsonSchemaJSON, jsonSchemaDummy, jsonSchemaLNF, jsonSchemaUniRec]
];
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
            modules: [[], [], []],
            overlay: null
        };
    }

    editCancel() {
        this.setState({
            overlay: null
        });
        console.log("Editing canceled");
    }

    newModuleOverlay(columnIndex, moduleIndex) {
        this.setState({
            overlay: (
                <Overlay
                    columnIndex={columnIndex}
                    moduleIndex={moduleIndex}
                    jsonSchema={moduleSchemas[columnIndex][moduleIndex]}
                    onCancel={this.editCancel.bind(this)}
                    onSuccess={this.addModule.bind(this)}
                />
            )
        });
        console.log("Editing a new module");
    }

    editModuleOverlay(columnIndex, index) {
        this.setState({
            overlay: (
                <Overlay
                    module={this.state.modules[columnIndex][index]}
                    columnIndex={columnIndex}
                    moduleIndex={moduleIndex}
                    jsonSchema={moduleSchemas[columnIndex][moduleIndex]}
                    onCancel={this.editCancel.bind(this)}
                    onSuccess={this.editModule.bind(this)}
                />
            )
        });
        console.log("Editing an existing module");
    }

    addModule(columnIndex, module) {
        var modules = this.state.modules;
        modules[columnIndex] = modules[columnIndex].concat(module);
        this.setState({
            modules: modules,
            overlay: null
        });
        console.log("New module added");
    }

    editModule(columnIndex, moduleIndex, module) {

    }

    removeModule(columnName, index) {
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
        console.log("Module removed");
    }

    renderXML() {
        var config = defaultConfig;
        config.ipfixcol2.inputPlugins.input = this.state.modules[0];
        config.ipfixcol2.intermediatePlugins.intermediate = this.state.modules[1];
        config.ipfixcol2.outputPlugins.output = this.state.modules[2];
        var xml = x2js.json2xml_str(config);
        return (
            <FormControl fullWidth>
                <TextField
                    label={"Config XML"}
                    multiline
                    InputProps={{
                        readOnly: true
                    }}
                    value={formatXml(xml)}
                />
                <FormHelperText>Read only</FormHelperText>
            </FormControl>
        );
    }

    render() {
        return (
            <div className="form">
                {this.state.overlay}
                <div className="mainLayer">
                    <FormColumn
                        key={columnNames[0]}
                        columnIndex={0}
                        parent={this}
                        modules={this.state.modules[0]}
                        color={colors[0]}
                        name={columnNames[0]}
                        modulesAvailable={moduleSchemas[0]}
                        addModule={this.newModuleOverlay.bind(this)}
                    />
                    <FormColumn
                        key={columnNames[1]}
                        columnIndex={1}
                        parent={this}
                        modules={this.state.modules[1]}
                        color={colors[1]}
                        name={columnNames[1]}
                        modulesAvailable={moduleSchemas[1]}
                        addModule={this.newModuleOverlay.bind(this)}
                    />
                    <FormColumn
                        key={columnNames[2]}
                        columnIndex={2}
                        parent={this}
                        modules={this.state.modules[2]}
                        color={colors[2]}
                        name={columnNames[2]}
                        modulesAvailable={moduleSchemas[2]}
                        addModule={this.newModuleOverlay.bind(this)}
                    />
                    {this.renderXML()}
                </div>
            </div>
        );
    }
}

class FormColumn extends React.Component {
    constructor(props) {
        super(props);
    }

    addModule = index => {
        this.props.addModule(this.props.columnIndex, index);
    };

    removeModule = (name, index) => {
        this.props.parent.removeModule(name, index);
    };

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
                                    moduleIndex={index}
                                    module={moduleAvailable}
                                    onAdd={this.addModule}
                                />
                            );
                        })}
                    </div>
                </div>
            </div>
        );
    }
}

class ModuleAvailable extends React.Component {
    handleAdd = () => {
        this.props.onAdd(this.props.moduleIndex);
    };

    render() {
        return (
            <button onClick={this.handleAdd}>{this.props.module.properties.plugin.const}</button>
        );
    }
}

class Module extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            detailVisible: false
        };
    }

    setDetailVisibile = () => {
        this.setState({ detailVisible: true });
        console.log("set visible");
    };

    setDetailHidden = () => {
        this.setState({ detailVisible: false });
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

// ========================================

ReactDOM.render(<App />, rootAppElement);
