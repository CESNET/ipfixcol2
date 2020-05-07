const {
    Allert,
    AppBar,
    //    Autocomplete, - in lab
    Badge,
    Button,
    Card,
    CardContent,
    CardActions,
    CardHeader,
    Collapse,
    Dialog,
    DialogActions,
    DialogContent,
    DialogTitle,
    Divider,
    ExpansionPanel,
    ExpansionPanelSummary,
    ExpansionPanelDetails,
    FormControl,
    FormControlLabel,
    FormGroup,
    FormHelperText,
    FormLabel,
    Grid,
    Icon,
    IconButton,
    Input,
    InputAdornment,
    InputLabel,
    List,
    ListItem,
    ListItemText,
    Menu,
    MenuItem,
    Select,
    Snackbar,
    SvgIcon,
    Switch,
    TextareaAutosize,
    TextField,
    Toolbar,
    Tooltip,
    //    TreeItem, - in lab
    //    TreeView, - in lab
    Typography,
} = MaterialUI;
// Obtain the root element
const rootAppElement = document.getElementById("configurator_app");
const colors = ["blue", "orange", "red"];
const columnNames = ["Input plugins", "Intermediate plugins", "Output plugins"];

let merge = (obj1, obj2) => {
    let target = {};
    // Merge the object into the target object
    let merger = (obj) => {
        for (let prop in obj) {
            if (obj.hasOwnProperty(prop)) {
                if (Object.prototype.toString.call(obj[prop]) === "[object Object]") {
                    // If we're doing a deep merge
                    // and the property is an object
                    target[prop] = merge(target[prop], obj[prop]);
                } else {
                    // Otherwise, do a regular merge
                    target[prop] = obj[prop];
                }
            }
        }
    };
    merger(obj1);
    merger(obj2);
    return target;
};

var config = {};
var pluginSchemas = [];
var nameValidationSchema;
var outputExtraParams;
var allPluginsExtraParams;

async function loadAppData() {
    config = await fetch("./config/config.json").then((response) => response.json());
    pluginSchemas = [
        loadSchemas(config.schemaLocations.input),
        loadSchemas(config.schemaLocations.intermediate),
        loadSchemas(config.schemaLocations.output),
    ];
    nameValidationSchema = await fetch(
        config.schemaLocations.special.path + config.schemaLocations.special.nameValidationSchema
    ).then((response) => response.json());
    outputExtraParams = await fetch(
        config.schemaLocations.special.path + config.schemaLocations.special.outputExtraParams
    ).then((response) => response.json());
    allPluginsExtraParams = await fetch(
        config.schemaLocations.special.path + config.schemaLocations.special.allPluginsExtraParams
    ).then((response) => response.json());
}

function loadSchemas(typeSchemaLocations) {
    var array = [];
    typeSchemaLocations.pluginSchemas.map((filename) => {
        fetch(typeSchemaLocations.path + filename) // not supported by Internet Explorer
            .then((response) => response.json())
            .then((json) => array.push(json));
    });
    return array;
}

const defaultConfig = {
    ipfixcol2: {
        inputPlugins: {
            input: [],
        },
        intermediatePlugins: {
            intermediate: [],
        },
        outputPlugins: {
            output: [],
        },
    },
};
const indentationTypes = [
    { name: "Space", character: " " },
    { name: "Tab", character: "\t" },
];
const indentationSpaces = { min: 0, max: 8 };

const x2js = new X2JS();
const ajv = new Ajv({ allErrors: true });

class App extends React.Component {
    render() {
        return <Form />;
    }
}

class Form extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            plugins: [[], [], []],
            overlay: null,
            snackbarOpen: false,
            snackbarText: "",
            snackbarType: null,
            settingsOpen: false,
            confirmDialogOpen: false,
            confirmDialogRemoveFunc: null,
            indentType: indentationTypes[0],
            indentNumber: 2,
            showConfirmationDialogs: true,
        };
    }

    findSchema(plugin, schemas) {
        var pluginSchema = schemas.find(
            (schema) => plugin.plugin === schema.properties.plugin.const
        );
        if (pluginSchema !== undefined) {
            return pluginSchema;
        }
        console.log("Schema not found!");
    }

    editCancel() {
        this.setState({
            overlay: null,
        });
        this.openSnackbar("Editing canceled");
        console.log("Editing canceled");
    }

    getSectionPluginNames(columnIndex) {
        return this.state.plugins[columnIndex].map((plugin) => plugin.name);
    }

    applySchemaExtensions(schema, columnIndex) {
        schema = merge(schema, allPluginsExtraParams);
        if (columnIndex == 2) {
            schema = merge(schema, outputExtraParams);
        }
        return schema;
    }

    newPluginOverlay(columnIndex, pluginIndex) {
        var extendedSchema = this.applySchemaExtensions(
            pluginSchemas[columnIndex][pluginIndex],
            columnIndex
        );
        this.setState({
            overlay: (
                <Overlay
                    columnIndex={columnIndex}
                    jsonSchema={extendedSchema}
                    pluginNames={this.getSectionPluginNames(columnIndex)}
                    onCancel={this.editCancel.bind(this)}
                    onSuccess={this.addPlugin.bind(this)}
                    XMLIndentType={this.state.indentType}
                    XMLIndentNumber={this.state.indentNumber}
                />
            ),
        });
        console.log("Editing a new plugin");
    }

    editPluginOverlay(columnIndex, index) {
        var plugin = this.state.plugins[columnIndex][index];
        var pluginJsonSchema = this.findSchema(plugin, pluginSchemas[columnIndex]);
        var extendedSchema = this.applySchemaExtensions(pluginJsonSchema, columnIndex);
        var pluginNames = JSON.parse(JSON.stringify(this.getSectionPluginNames(columnIndex)));
        pluginNames.splice(index, 1);
        this.setState({
            overlay: (
                <Overlay
                    plugin={plugin}
                    columnIndex={columnIndex}
                    index={index}
                    jsonSchema={extendedSchema}
                    pluginNames={pluginNames}
                    onCancel={this.editCancel.bind(this)}
                    onSuccess={this.editPlugin.bind(this)}
                    XMLIndentType={this.state.indentType}
                    XMLIndentNumber={this.state.indentNumber}
                    showConfirmationDialogs={this.state.showConfirmationDialogs}
                />
            ),
        });
        console.log("Editing an existing plugin");
    }

    addPlugin(columnIndex, plugin) {
        var plugins = this.state.plugins;
        plugins[columnIndex] = plugins[columnIndex].concat(plugin);
        this.setState({
            plugins: plugins,
            overlay: null,
        });
        this.openSnackbar("New plugin added");
        console.log("New plugin added");
    }

    editPlugin(columnIndex, index, plugin) {
        var plugins = this.state.plugins;
        plugins[columnIndex][index] = plugin;
        this.setState({
            plugins: plugins,
            overlay: null,
        });
        this.openSnackbar("Plugin edited");
        console.log("Plugin edited");
    }

    removePluginConfirm(columnIndex, index) {
        if (this.state.showConfirmationDialogs) {
            this.setState({
                confirmDialogOpen: true,
                confirmDialogRemoveFunc: this.removePlugin.bind(this, columnIndex, index),
            });
        } else {
            this.removePlugin(columnIndex, index);
        }
    }

    removePlugin(columnIndex, index) {
        var plugins = this.state.plugins;
        plugins[columnIndex].splice(index, 1);
        this.setState({
            plugins: plugins,
        });
        this.openSnackbar("Plugin removed");
        console.log("Plugin removed");
    }

    handleConfirmDialogClose(confirmed) {
        this.setState({ confirmDialogOpen: false });
        if (confirmed) {
            this.state.confirmDialogRemoveFunc();
        }
    }

    createConfigXML() {
        var config = JSON.parse(JSON.stringify(defaultConfig));
        if (this.state.plugins[0].length === 0) {
            delete config.ipfixcol2.inputPlugins;
        } else {
            config.ipfixcol2.inputPlugins.input = this.state.plugins[0];
        }
        if (this.state.plugins[1].length === 0) {
            delete config.ipfixcol2.intermediatePlugins;
        } else {
            config.ipfixcol2.intermediatePlugins.intermediate = this.state.plugins[1];
        }
        if (this.state.plugins[2].length === 0) {
            delete config.ipfixcol2.outputPlugins;
        } else {
            config.ipfixcol2.outputPlugins.output = this.state.plugins[2];
        }
        var xml = x2js.json2xml_str(config);
        return formatXml(xml, this.state.indentType.character, this.state.indentNumber);
    }

    renderXML() {
        return (
            <FormControl fullWidth>
                <TextField
                    label={"Config XML"}
                    multiline
                    InputProps={{
                        readOnly: true,
                    }}
                    value={this.createConfigXML()}
                />
                <FormHelperText>Read only</FormHelperText>
            </FormControl>
        );
    }

    printColoredXML() {
        var input = false;
        var intermediate = false;
        var output = false;
        var formatedXML = this.createConfigXML();
        var parts = formatedXML.split(
            /\s*(<inputPlugins>|<\/inputPlugins>|<intermediatePlugins>|<\/intermediatePlugins>|<outputPlugins>|<\/outputPlugins>)\r\n/
        );
        var indentation = this.state.indentType.character.repeat(this.state.indentNumber);
        return (
            <div className={"XMLPrint"}>
                {parts.map((part, index) => {
                    if (index === 0 || index === parts.length - 1) {
                        return <pre key={index}>{part}</pre>;
                    }
                    if (part == "<inputPlugins>") {
                        input = true;
                        return <pre key={index}>{indentation + "<inputPlugins>"}</pre>;
                    }
                    if (part == "<intermediatePlugins>") {
                        intermediate = true;
                        return <pre key={index}>{indentation + "<intermediatePlugins>"}</pre>;
                    }
                    if (part == "<outputPlugins>") {
                        output = true;
                        return <pre key={index}>{indentation + "<outputPlugins>"}</pre>;
                    }
                    if (part == "</inputPlugins>") {
                        input = false;
                        return <pre key={index}>{indentation + "</inputPlugins>"}</pre>;
                    }
                    if (part == "</intermediatePlugins>") {
                        intermediate = false;
                        return <pre key={index}>{indentation + "</intermediatePlugins>"}</pre>;
                    }
                    if (part == "</outputPlugins>") {
                        output = false;
                        return <pre key={index}>{indentation + "</outputPlugins>"}</pre>;
                    }
                    if (input) {
                        return (
                            <pre key={index} className={"input"}>
                                {part}
                            </pre>
                        );
                    }
                    if (intermediate) {
                        return (
                            <pre key={index} className={"intermediate"}>
                                {part}
                            </pre>
                        );
                    }
                    if (output) {
                        return (
                            <pre key={index} className={"output"}>
                                {part}
                            </pre>
                        );
                    }
                })}
            </div>
        );
    }

    openSnackbar(text) {
        this.setState({
            snackbarOpen: true,
            snackbarText: text,
        });
    }

    closeSnackbar(event, reason) {
        if (reason === "clickaway") {
            return;
        }

        this.setState({
            snackbarOpen: false,
        });
    }

    openSettings() {
        this.setState({
            settingsOpen: true,
        });
    }

    closeSettings() {
        this.setState({
            settingsOpen: false,
        });
    }

    changeSettings(type, number, showConfirmationDialogs) {
        this.setState({
            indentType: type,
            indentNumber: number,
            showConfirmationDialogs: showConfirmationDialogs,
        });
    }

    download() {
        var element = document.createElement("a");
        element.style.display = "none";
        element.setAttribute(
            "href",
            "data:text/xml;charset=utf-8," + encodeURIComponent(this.createConfigXML())
        );
        element.setAttribute("download", "config.xml");

        document.body.appendChild(element);
        element.click();
        document.body.removeChild(element);
    }

    render() {
        return (
            <React.Fragment>
                <AppBar position="sticky">
                    <Toolbar variant="dense">
                        <Typography variant="h6" color="inherit" className="title">
                            IPFIXcol2 configurator
                        </Typography>
                        <Tooltip title={"IPFIXcol2 GitHub"} arrow placement={"bottom"}>
                            <IconButton
                                color="inherit"
                                target={"_blank"}
                                href={"https://github.com/CESNET/ipfixcol2/"}
                            >
                                <SvgIcon>
                                    <path d="M12.007 0C6.12 0 1.1 4.27.157 10.08c-.944 5.813 2.468 11.45 8.054 13.312.19.064.397.033.555-.084.16-.117.25-.304.244-.5v-2.042c-3.33.735-4.037-1.56-4.037-1.56-.22-.726-.694-1.35-1.334-1.756-1.096-.75.074-.735.074-.735.773.103 1.454.557 1.846 1.23.694 1.21 2.23 1.638 3.45.96.056-.61.327-1.178.766-1.605-2.67-.3-5.462-1.335-5.462-6.002-.02-1.193.42-2.35 1.23-3.226-.327-1.015-.27-2.116.166-3.09 0 0 1.006-.33 3.3 1.23 1.966-.538 4.04-.538 6.003 0 2.295-1.5 3.3-1.23 3.3-1.23.445 1.006.49 2.144.12 3.18.81.877 1.25 2.033 1.23 3.226 0 4.607-2.805 5.627-5.476 5.927.578.583.88 1.386.825 2.206v3.29c-.005.2.092.393.26.507.164.115.377.14.565.063 5.568-1.88 8.956-7.514 8.007-13.313C22.892 4.267 17.884.007 12.008 0z" />
                                </SvgIcon>
                            </IconButton>
                        </Tooltip>
                        <Tooltip title={"Download file"} arrow placement={"bottom"}>
                            <IconButton color="inherit" onClick={this.download.bind(this)}>
                                <Icon>get_app</Icon>
                            </IconButton>
                        </Tooltip>
                        <Tooltip title={"Settings"} arrow placement={"bottom"}>
                            <IconButton color="inherit" onClick={this.openSettings.bind(this)}>
                                <Icon>settings</Icon>
                            </IconButton>
                        </Tooltip>
                    </Toolbar>
                </AppBar>
                <Settings
                    open={this.state.settingsOpen}
                    indentType={this.state.indentType}
                    indentNumber={this.state.indentNumber}
                    showConfirmationDialogs={this.state.showConfirmationDialogs}
                    onChange={this.changeSettings.bind(this)}
                    onClose={this.closeSettings.bind(this)}
                />
                <Dialog
                    className={"settings"}
                    open={this.state.confirmDialogOpen}
                    fullWidth={false}
                    maxWidth={"sm"}
                >
                    <DialogTitle>{"Are you sure?"}</DialogTitle>
                    <Divider />
                    <DialogContent dividers>
                        <Typography>Plugin and its settings will be lost.</Typography>
                        <Typography>Do you want to proceed?</Typography>
                    </DialogContent>
                    <DialogActions>
                        <Button
                            autoFocus
                            color="primary"
                            onClick={this.handleConfirmDialogClose.bind(this, false)}
                        >
                            {"Cancel"}
                        </Button>
                        <Button
                            color="primary"
                            onClick={this.handleConfirmDialogClose.bind(this, true)}
                        >
                            {"Continue"}
                        </Button>
                    </DialogActions>
                </Dialog>
                <div className="form">
                    {this.state.overlay}
                    <div className="mainLayer">
                        <FormColumn
                            key={columnNames[0]}
                            columnIndex={0}
                            plugins={this.state.plugins[0]}
                            color={colors[0]}
                            name={columnNames[0]}
                            pluginsAvailable={pluginSchemas[0]}
                            addPlugin={this.newPluginOverlay.bind(this)}
                            editPlugin={this.editPluginOverlay.bind(this)}
                            removePlugin={this.removePluginConfirm.bind(this)}
                        />
                        <FormColumn
                            key={columnNames[1]}
                            columnIndex={1}
                            plugins={this.state.plugins[1]}
                            color={colors[1]}
                            name={columnNames[1]}
                            pluginsAvailable={pluginSchemas[1]}
                            addPlugin={this.newPluginOverlay.bind(this)}
                            editPlugin={this.editPluginOverlay.bind(this)}
                            removePlugin={this.removePluginConfirm.bind(this)}
                        />
                        <FormColumn
                            key={columnNames[2]}
                            columnIndex={2}
                            plugins={this.state.plugins[2]}
                            color={colors[2]}
                            name={columnNames[2]}
                            pluginsAvailable={pluginSchemas[2]}
                            addPlugin={this.newPluginOverlay.bind(this)}
                            editPlugin={this.editPluginOverlay.bind(this)}
                            removePlugin={this.removePluginConfirm.bind(this)}
                        />
                    </div>
                    {this.printColoredXML()}
                    <Snackbar
                        anchorOrigin={{
                            vertical: "bottom",
                            horizontal: "center",
                        }}
                        open={this.state.snackbarOpen}
                        autoHideDuration={5000}
                        onClose={this.closeSnackbar.bind(this)}
                        message={this.state.snackbarText}
                        action={
                            <React.Fragment>
                                <IconButton
                                    size="small"
                                    aria-label="close"
                                    color="inherit"
                                    onClick={this.closeSnackbar.bind(this)}
                                >
                                    <Icon fontSize="small">close</Icon>
                                </IconButton>
                            </React.Fragment>
                        }
                    />
                </div>
            </React.Fragment>
        );
    }
}

class FormColumn extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            anchorEl: null,
            expanded: true,
        };
    }

    addPlugin(pluginIndex) {
        this.setState({ anchorEl: null });
        this.props.addPlugin(this.props.columnIndex, pluginIndex);
    }

    removePlugin(index) {
        this.props.removePlugin(this.props.columnIndex, index);
    }

    editPlugin(index) {
        this.props.editPlugin(this.props.columnIndex, index);
    }

    handleMenuClick(event) {
        this.setState({ anchorEl: event.currentTarget });
    }
    handleMenuClose() {
        this.setState({ anchorEl: null });
    }

    render() {
        var plugins = "";
        if (this.props.plugins.length > 0) {
            plugins = (
                <CardContent>
                    {this.props.plugins.map((plugin, index) => {
                        return (
                            <Plugin
                                key={index}
                                index={index}
                                plugin={plugin}
                                onRemove={this.removePlugin.bind(this)}
                                onEdit={this.editPlugin.bind(this)}
                            />
                        );
                    })}
                </CardContent>
            );
        }
        var addMenu = (
            <React.Fragment>
                <Button
                    variant="outlined"
                    color="primary"
                    aria-controls="simple-menu"
                    aria-haspopup="true"
                    onClick={this.handleMenuClick.bind(this)}
                >
                    Add plugin
                </Button>
                <Menu
                    id="simple-menu"
                    anchorEl={this.state.anchorEl}
                    keepMounted
                    open={Boolean(this.state.anchorEl)}
                    onClose={this.handleMenuClose.bind(this)}
                >
                    {this.props.pluginsAvailable.map((pluginAvailable, index) => {
                        return (
                            <PluginAvailable
                                key={index}
                                pluginIndex={index}
                                plugin={pluginAvailable}
                                onAdd={this.addPlugin.bind(this)}
                            />
                        );
                    })}
                </Menu>
            </React.Fragment>
        );
        return (
            <Card className={"column " + this.props.color}>
                <CardHeader className={"title"} title={this.props.name} />
                <Divider />
                {plugins}
                <Divider />
                <CardActions disableSpacing>{addMenu}</CardActions>
            </Card>
        );
    }
}

class PluginAvailable extends React.Component {
    handleAdd() {
        this.props.onAdd(this.props.pluginIndex);
    }

    render() {
        return <MenuItem onClick={this.handleAdd.bind(this)}>{this.props.plugin.title}</MenuItem>;
    }
}

class Plugin extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            detailVisible: false,
        };
    }

    handleEdit() {
        this.props.onEdit(this.props.index);
    }

    setDetailVisibile() {
        this.setState({ detailVisible: true });
    }

    setDetailHidden() {
        this.setState({ detailVisible: false });
    }

    render() {
        return (
            <ExpansionPanel>
                <ExpansionPanelSummary
                    expandIcon={<Icon>expand_more</Icon>}
                    aria-controls="panel1c-content"
                    id="panel1c-header"
                >
                    <Typography className={"title"}>{this.props.plugin.name}</Typography>
                    <FormControlLabel
                        onClick={this.handleEdit.bind(this)}
                        control={
                            <IconButton aria-label="edit">
                                <Icon>edit</Icon>
                            </IconButton>
                        }
                        label={""}
                    />
                    <FormControlLabel
                        onClick={this.props.onRemove}
                        control={
                            <IconButton aria-label="delete">
                                <Icon>delete</Icon>
                            </IconButton>
                        }
                    />
                </ExpansionPanelSummary>
                <ExpansionPanelDetails>
                    <PropsItem plugin={this.props.plugin} />
                </ExpansionPanelDetails>
            </ExpansionPanel>
        );
    }
}

class PropsItem extends React.Component {
    render() {
        if (Array.isArray(this.props.plugin)) {
            return (
                <React.Fragment>
                    {this.props.plugin.map((item, index) => {
                        return <PropsItem key={index} name={this.props.name} plugin={item} />;
                    })}
                </React.Fragment>
            );
        }
        if (typeof this.props.plugin === "object") {
            return Object.keys(this.props.plugin).length == 0 ? (
                <ListItem button>
                    <ListItemText primary={this.props.name} secondary={"No parameters"} />
                </ListItem>
            ) : (
                <React.Fragment>
                    {this.props.name !== undefined ? (
                        <ListItem button>
                            <ListItemText primary={this.props.name} />
                        </ListItem>
                    ) : (
                        ""
                    )}
                    <ListItem>
                        <List dense>
                            {Object.keys(this.props.plugin).map((propertyName) => {
                                return (
                                    <PropsItem
                                        key={propertyName}
                                        name={propertyName}
                                        plugin={this.props.plugin[propertyName]}
                                    />
                                );
                            })}
                        </List>
                    </ListItem>
                </React.Fragment>
            );
        }
        if (typeof this.props.plugin === "boolean") {
            return (
                <ListItem button>
                    <ListItemText
                        primary={this.props.name}
                        secondary={this.props.plugin.toString()}
                    />
                </ListItem>
            );
        }
        return (
            <ListItem button>
                <ListItemText primary={this.props.name} secondary={this.props.plugin} />
            </ListItem>
        );
    }
}

class Settings extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            indentNumber: this.props.indentNumber,
            confirmDialodOpen: false,
        };
    }

    handleChangeType(event) {
        if (event.target.value === indentationTypes[1].name) {
            this.props.onChange(indentationTypes[1], 1, this.props.showConfirmationDialogs);
        } else if (this.props.indentType === indentationTypes[1]) {
            this.props.onChange(indentationTypes[0], 2, this.props.showConfirmationDialogs);
        } else {
            this.props.onChange(
                indentationTypes[0],
                this.props.indentNumber,
                this.props.showConfirmationDialogs
            );
        }
    }

    handleChangeNumber(event) {
        var value = Number(event.target.value);
        if (value < indentationSpaces.min) {
            this.setState({ indentNumber: "" });
            return;
        } else if (value > indentationSpaces.max) {
            value = indentationSpaces.max;
        }
        this.setState({ indentNumber: value });
        this.props.onChange(this.props.indentType, value, this.props.showConfirmationDialogs);
    }

    handleChangeShowConfirmationDialogs() {
        this.props.onChange(
            this.props.indentType,
            this.props.indentNumber,
            !this.props.showConfirmationDialogs
        );
    }

    handleOnClose() {
        this.setState({ indentNumber: this.props.indentNumber });
        this.props.onClose();
    }

    render() {
        return (
            <Dialog
                className={"settings"}
                open={this.props.open}
                fullWidth={true}
                maxWidth={"xs"}
                onEscapeKeyDown={this.handleOnClose.bind(this)}
                onBackdropClick={this.handleOnClose.bind(this)}
            >
                <DialogTitle>{"Settings"}</DialogTitle>
                <Divider />
                <DialogContent dividers>
                    <Grid container direction="column" spacing={2}>
                        <FormControl fullWidth>
                            <FormLabel>{"Indentation character"}</FormLabel>
                            <Select
                                className={"select"}
                                value={this.props.indentType.name}
                                onChange={this.handleChangeType.bind(this)}
                                inputProps={{ tabIndex: 1 }}
                            >
                                {indentationTypes.map((indentType) => {
                                    return (
                                        <MenuItem key={indentType.name} value={indentType.name}>
                                            {indentType.name}
                                        </MenuItem>
                                    );
                                })}
                            </Select>
                        </FormControl>
                        <FormControl>
                            <FormLabel>
                                {"Number of " + this.props.indentType.name.toLowerCase() + "s"}
                            </FormLabel>
                            <Input
                                className={"select"}
                                type={"number"}
                                name={"numberOfIndentChars"}
                                value={this.state.indentNumber}
                                disabled={this.props.indentType === indentationTypes[1]}
                                inputProps={{
                                    min: indentationSpaces.min,
                                    max: indentationSpaces.max,
                                    step: 1,
                                    tabIndex: 2,
                                }}
                                onChange={this.handleChangeNumber.bind(this)}
                            />
                        </FormControl>
                        <FormControl>
                            <FormLabel>Show confirmation dialogs</FormLabel>
                            <Grid component="label" container alignItems="center" spacing={0}>
                                <Grid item>False</Grid>
                                <Grid item>
                                    <Switch
                                        checked={this.props.showConfirmationDialogs}
                                        onChange={this.handleChangeShowConfirmationDialogs.bind(
                                            this
                                        )}
                                    />
                                </Grid>
                                <Grid item>True</Grid>
                            </Grid>
                        </FormControl>
                    </Grid>
                </DialogContent>
                <DialogActions>
                    <Button
                        variant="outlined"
                        color="primary"
                        onClick={this.handleOnClose.bind(this)}
                        tabIndex={3}
                    >
                        {"Close"}
                    </Button>
                </DialogActions>
            </Dialog>
        );
    }
}

// ========================================

async function startApp() {
    await loadAppData();

    ReactDOM.render(<App />, rootAppElement);
}

startApp();
