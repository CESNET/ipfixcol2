
/**
 * Saves cookie into browser
 * 
 * @param {string} cookieName 
 * @param {*} value 
 */
function setCookie(cookieName, value) {
    let d = new Date();
    d.setTime(d.getTime() + COOKIE_EXPIRATION_DAYS * 24 * 60 * 60 * 1000);
    let expires = "expires=" + d.toUTCString();
    document.cookie = cookieName + "=" + value + ";" + expires + ";path=/";
}

/**
 * Gets cookie from browser
 * 
 * @param {string} cookieName 
 */
function getCookie(cookieName) {
    let name = cookieName + "=";
    let parts = document.cookie.split(";");
    for (let i in parts) {
        let part = parts[i].trim();
        if (part.indexOf(name) == 0) {
            return part.substring(name.length, part.length);
        }
    }
    return undefined;
}

/**
 * Merges two objects
 * 
 * @param {object} obj1 
 * @param {object} obj2 
 */
function merge(obj1, obj2) {
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


/**
 * Component representing apps entry point.
 */
class App extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            config: undefined,
            pluginSchemas: undefined,
            specialSchemas: undefined,
        };
        this.loadAppData();
    }

    /**
     * Downloads apps data
     */
    async loadAppData() {
        let config = await fetch("./config/config.json").then((response) => response.json());
        let pluginSchemas = [
            await this.loadSchemas(config.schemaLocations.input),
            await this.loadSchemas(config.schemaLocations.intermediate),
            await this.loadSchemas(config.schemaLocations.output),
        ];
        let specialSchemas = {
            nameValidation: undefined,
            outputExtraParams: undefined,
            allPluginsExtraParams: undefined,
            generalStructure: undefined,
        };
        specialSchemas.nameValidation = await fetch(
            config.schemaLocations.special.path +
                config.schemaLocations.special.nameValidationSchema
        ).then((response) => response.json());
        specialSchemas.outputExtraParams = await fetch(
            config.schemaLocations.special.path + config.schemaLocations.special.outputExtraParams
        ).then((response) => response.json());
        specialSchemas.allPluginsExtraParams = await fetch(
            config.schemaLocations.special.path +
                config.schemaLocations.special.allPluginsExtraParams
        ).then((response) => response.json());
        specialSchemas.generalStructure = await fetch(
            config.schemaLocations.special.path + config.schemaLocations.special.generalStructure
        ).then((response) => response.json());
        this.setState({
            config: config,
            pluginSchemas: pluginSchemas,
            specialSchemas: specialSchemas,
        });
    }

    /**
     * Downloads schemas from the location provided.
     * @param {object} typeSchemaLocations 
     */
    async loadSchemas(typeSchemaLocations) {
        let array = [];
        typeSchemaLocations.pluginSchemas.map((filename) => {
            fetch(typeSchemaLocations.path + filename) // not supported by Internet Explorer
                .then((response) => response.json())
                .then((json) => array.push(json));
        });
        return array;
    }

    render() {
        let form =
            this.state.config !== undefined &&
            this.state.pluginSchemas !== undefined &&
            this.state.specialSchemas !== undefined ? (
                <Form
                    config={this.state.config}
                    pluginSchemas={this.state.pluginSchemas}
                    specialSchemas={this.state.specialSchemas}
                />
            ) : (
                <ThemeProvider theme={MAIN_THEME}>
                    <LinearProgress />
                </ThemeProvider>
            );
        return form;
    }
}

/**
 * Component representing main app page.
 */
class Form extends React.Component {
    constructor(props) {
        super(props);
        let configObj = JSON.parse(JSON.stringify(DEFAULT_CONFIG));
        let { valid, errors } = this.validateConfig(configObj);
        let cookieSettings = {
            indentType: getCookie("indentType"),
            indentNumber: getCookie("indentNumber"),
            showConfirmationDialogs: getCookie("showConfirmationDialogs"),
        };
        if (
            cookieSettings.indentType !== undefined &&
            cookieSettings.indentNumber !== undefined &&
            cookieSettings.showConfirmationDialogs !== undefined
        ) {
            if (cookieSettings.indentType == "Space") {
                cookieSettings.indentType = INDENTATION_TYPES[0];
            } else {
                cookieSettings.indentType = INDENTATION_TYPES[1];
            }
            cookieSettings.indentNumber = Number(cookieSettings.indentNumber);
            cookieSettings.showConfirmationDialogs =
                cookieSettings.showConfirmationDialogs === "true";
        } else {
            cookieSettings = JSON.parse(JSON.stringify(DEFAULT_SETTINGS));
        }
        this.state = {
            plugins: [[], [], []],
            overlay: null,
            snackbarOpen: false,
            snackbarText: "",
            snackbarType: null,
            settingsOpen: false,
            confirmDialogOpen: false,
            confirmDialogRemoveFunc: null,
            downloadDialogOpen: false,
            indentType: cookieSettings.indentType,
            indentNumber: cookieSettings.indentNumber,
            showConfirmationDialogs: cookieSettings.showConfirmationDialogs,
            valid: valid,
            errors: errors,
            configObj: configObj,
        };
    }

    /**
     * Finds correct schema for the plugin.
     * @param {object} plugin 
     * @param {object[]} schemas 
     */
    findSchema(plugin, schemas) {
        let pluginSchema = schemas.find(
            (schema) => plugin.plugin === schema.properties.plugin.const
        );
        if (pluginSchema !== undefined) {
            return pluginSchema;
        }
        console.log("Schema not found!");
    }

    /**
     * Shows snackbar
     */
    editCancel() {
        this.setState({
            overlay: null,
        });
        this.openSnackbar("Editing canceled");
    }

    /**
     * Gets an array of names of all the section plugins
     * @param {number} sectionIndex 
     */
    getSectionPluginNames(sectionIndex) {
        return this.state.plugins[sectionIndex].map((plugin) => plugin.name);
    }

    /**
     * Extends schema by extra parameters
     * @param {object} schema 
     * @param {number} sectionIndex 
     */
    applySchemaExtensions(schema, sectionIndex) {
        schema = merge(schema, this.props.specialSchemas.allPluginsExtraParams);
        if (sectionIndex == 2) {
            schema = merge(schema, this.props.specialSchemas.outputExtraParams);
        }
        return schema;
    }

    /**
     * Opens an Overlay to add a new plugin
     * @param {number} sectionIndex 
     * @param {number} pluginIndex 
     */
    newPluginOverlay(sectionIndex, pluginIndex) {
        let extendedSchema = this.applySchemaExtensions(
            this.props.pluginSchemas[sectionIndex][pluginIndex],
            sectionIndex
        );
        this.setState({
            overlay: (
                <Overlay
                    sectionIndex={sectionIndex}
                    jsonSchema={extendedSchema}
                    nameValidationSchema={this.props.specialSchemas.nameValidation}
                    pluginNames={this.getSectionPluginNames(sectionIndex)}
                    onCancel={this.editCancel.bind(this)}
                    onSuccess={this.addPlugin.bind(this)}
                    XMLIndentType={this.state.indentType}
                    XMLIndentNumber={this.state.indentNumber}
                    showConfirmationDialogs={this.state.showConfirmationDialogs}
                />
            ),
        });
    }

    /**
     * Opens an Overlay to edit an existing plugin
     * @param {number} sectionIndex 
     * @param {number} index 
     */
    editPluginOverlay(sectionIndex, index) {
        let plugin = this.state.plugins[sectionIndex][index];
        let pluginJsonSchema = this.findSchema(plugin, this.props.pluginSchemas[sectionIndex]);
        let extendedSchema = this.applySchemaExtensions(pluginJsonSchema, sectionIndex);
        let pluginNames = JSON.parse(JSON.stringify(this.getSectionPluginNames(sectionIndex)));
        pluginNames.splice(index, 1);
        this.setState({
            overlay: (
                <Overlay
                    plugin={plugin}
                    sectionIndex={sectionIndex}
                    index={index}
                    jsonSchema={extendedSchema}
                    nameValidationSchema={this.props.specialSchemas.nameValidation}
                    pluginNames={pluginNames}
                    onCancel={this.editCancel.bind(this)}
                    onSuccess={this.editPlugin.bind(this)}
                    XMLIndentType={this.state.indentType}
                    XMLIndentNumber={this.state.indentNumber}
                    showConfirmationDialogs={this.state.showConfirmationDialogs}
                />
            ),
        });
    }

    /**
     * Adds a new plugin to the section
     * @param {number} sectionIndex 
     * @param {object} plugin 
     */
    addPlugin(sectionIndex, plugin) {
        let plugins = this.state.plugins;
        plugins[sectionIndex] = plugins[sectionIndex].concat(plugin);
        this.setState({
            plugins: plugins,
            overlay: null,
        });
        this.openSnackbar("New plugin added");
        this.updateConfigObj();
    }

    /**
     * Edits an existing plugin
     * @param {number} sectionIndex 
     * @param {number} index 
     * @param {object} plugin 
     */
    editPlugin(sectionIndex, index, plugin) {
        let plugins = this.state.plugins;
        plugins[sectionIndex][index] = plugin;
        this.setState({
            plugins: plugins,
            overlay: null,
        });
        this.openSnackbar("Plugin edited");
        this.updateConfigObj();
    }

    /**
     * Shows remove plugin confirm dialog
     * @param {number} sectionIndex 
     * @param {number} index 
     */
    removePluginConfirm(sectionIndex, index) {
        if (this.state.showConfirmationDialogs) {
            this.setState({
                confirmDialogOpen: true,
                confirmDialogRemoveFunc: this.removePlugin.bind(this, sectionIndex, index),
            });
        } else {
            this.removePlugin(sectionIndex, index);
        }
    }

    /**
     * Removes a plugin
     * @param {number} sectionIndex 
     * @param {number} index 
     */
    removePlugin(sectionIndex, index) {
        let plugins = this.state.plugins;
        plugins[sectionIndex].splice(index, 1);
        this.setState({
            plugins: plugins,
        });
        this.openSnackbar("Plugin removed");
        this.updateConfigObj();
    }

    /**
     * Closes remove plugin confirm dialog
     * @param {boolean} confirmed 
     */
    handleConfirmDialogClose(confirmed) {
        this.setState({ confirmDialogOpen: false });
        if (confirmed) {
            this.state.confirmDialogRemoveFunc();
        }
    }

    /**
     * Creates an object with all the plugins configuration
     */
    createConfigObj() {
        let configObj = JSON.parse(JSON.stringify(DEFAULT_CONFIG));
        configObj.ipfixcol2.inputPlugins.input = this.state.plugins[0];
        configObj.ipfixcol2.intermediatePlugins.intermediate = this.state.plugins[1];
        configObj.ipfixcol2.outputPlugins.output = this.state.plugins[2];
        return configObj;
    }

    /**
     * Validates a general structure of the configuration object
     * @param {object} configObj 
     */
    validateConfig(configObj) {
        configObj = JSON.parse(JSON.stringify(configObj));
        this.removeEmptyConfigIntermediatePluginType(configObj);
        let valid = AJV.validate(this.props.specialSchemas.generalStructure, configObj);
        let errors = undefined;
        if (!valid) {
            errors = JSON.parse(JSON.stringify(AJV.errors));
        }
        console.log("configObj valid: " + valid);
        return {
            valid: valid,
            errors: errors,
        };
    }

    /**
     * Updates configuration object, its valid status and list of errors
     */
    updateConfigObj() {
        let configObj = this.createConfigObj();
        let { valid, errors } = this.validateConfig(configObj);
        this.setState({
            configObj: configObj,
            valid: valid,
            errors: errors,
        });
    }

    /**
     * Creates XML string from the configuration object
     */
    createConfigXML() {
        let configObj = JSON.parse(JSON.stringify(this.state.configObj));
        this.removeEmptyConfigPluginTypes(configObj);
        let xml = X2JS.json2xml_str(configObj);
        return formatXml(xml, this.state.indentType.character, this.state.indentNumber);
    }

    /**
     * Removes an empty intermediate plugin category from the configuration file
     * @param {object} configObj 
     */
    removeEmptyConfigIntermediatePluginType(configObj) {
        if (configObj.ipfixcol2.intermediatePlugins.intermediate.length == 0) {
            delete configObj.ipfixcol2.intermediatePlugins;
        }
    }

    /**
     * Removes empty parts from the configuration file
     * @param {object} configObj 
     */
    removeEmptyConfigPluginTypes(configObj) {
        if (configObj.ipfixcol2.inputPlugins.input.length == 0) {
            delete configObj.ipfixcol2.inputPlugins.input;
        }
        this.removeEmptyConfigIntermediatePluginType(configObj);
        if (configObj.ipfixcol2.outputPlugins.output.length == 0) {
            delete configObj.ipfixcol2.outputPlugins.output;
        }
    }

    /**
     * Returns colored XML print
     */
    printColoredXML() {
        let input = false;
        let intermediate = false;
        let output = false;
        let formatedXML = this.createConfigXML();
        let parts = formatedXML.split(
            /\s*(<inputPlugins>|<\/inputPlugins>|<intermediatePlugins>|<\/intermediatePlugins>|<outputPlugins>|<\/outputPlugins>)\r\n/
        );
        let indentation = this.state.indentType.character.repeat(this.state.indentNumber);
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

    /**
     * Shows a snackbar with the text
     * @param {string} text 
     */
    openSnackbar(text) {
        this.setState({
            snackbarOpen: true,
            snackbarText: text,
        });
    }

    /**
     * Closes a snackbar
     * @param {*} event 
     * @param {*} reason 
     */
    closeSnackbar(event, reason) {
        if (reason === "clickaway") {
            return;
        }

        this.setState({
            snackbarOpen: false,
        });
    }

    /**
     * Opens settings
     */
    openSettings() {
        this.setState({
            settingsOpen: true,
        });
    }

    /**
     * Closes Settings
     */
    closeSettings() {
        this.setState({
            settingsOpen: false,
        });
    }

    /**
     * Saves settings
     * @param {object} type 
     * @param {number} number 
     * @param {boolean} showConfirmationDialogs 
     */
    changeSettings(type, number, showConfirmationDialogs) {
        let cookieSettings = {
            indentType: type,
            indentNumber: number,
            showConfirmationDialogs: showConfirmationDialogs,
        };
        setCookie("indentType", type.name);
        setCookie("indentNumber", number.toString());
        setCookie("showConfirmationDialogs", showConfirmationDialogs.toString());
        this.setState(cookieSettings);
    }

    /**
     * Closes a download dialog
     */
    handleDownloadDialogClose() {
        this.setState({
            downloadDialogOpen: false
        });
    }

    /**
     * Shows a download dialog if conditions are met
     */
    requestDownload() {
        if (this.state.valid || !this.state.showConfirmationDialogs) {
            this.download();
            return;
        }
        this.setState({
            downloadDialogOpen: true
        });
    }

    /**
     * Creates config.xml file and start downloading
     */
    download() {
        let element = document.createElement("a");
        element.style.display = "none";
        element.setAttribute(
            "href",
            "data:text/xml;charset=utf-8," + encodeURIComponent(this.createConfigXML())
        );
        element.setAttribute("download", "config.xml");

        document.body.appendChild(element);
        element.click();
        document.body.removeChild(element);
        this.handleDownloadDialogClose();
    }

    render() {
        let mainLayer = (
            <div className="mainLayer">
                {[0, 1, 2].map((i) => {
                    let columnErrors = [];
                    if (this.state.errors !== undefined) {
                        columnErrors = Object.values(this.state.errors).filter((error) => {
                            if (error.dataPath == COLUMN_DATA_PATH[i]) {
                                return true;
                            }
                            return false;
                        });
                    }
                    return (
                        <PluginTypeCard
                            key={COLUMN_NAMES[i]}
                            sectionIndex={i}
                            plugins={this.state.plugins[i]}
                            errors={columnErrors}
                            color={COLORS[i]}
                            name={COLUMN_NAMES[i]}
                            pluginsAvailable={this.props.pluginSchemas[i]}
                            addPlugin={this.newPluginOverlay.bind(this)}
                            editPlugin={this.editPluginOverlay.bind(this)}
                            removePlugin={this.removePluginConfirm.bind(this)}
                        />
                    );
                })}
            </div>
        );
        return (
            <React.Fragment>
                <ThemeProvider theme={MAIN_THEME}>
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
                                <IconButton color="inherit" onClick={this.requestDownload.bind(this)}>
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
                    <Dialog open={this.state.confirmDialogOpen} fullWidth={false} maxWidth={"sm"}>
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
                    <Dialog open={this.state.downloadDialogOpen} fullWidth={false} maxWidth={"sm"}>
                        <DialogTitle>{"The configuration is not valid."}</DialogTitle>
                        <Divider />
                        <DialogContent dividers>
                            <Typography>The configuration is not valid.</Typography>
                            <Typography>Do you want to download it anyway?</Typography>
                        </DialogContent>
                        <DialogActions>
                            <Button
                                autoFocus
                                color="primary"
                                onClick={this.handleDownloadDialogClose.bind(this, false)}
                            >
                                {"Cancel"}
                            </Button>
                            <Button
                                color="primary"
                                onClick={this.download.bind(this, true)}
                            >
                                {"Download"}
                            </Button>
                        </DialogActions>
                    </Dialog>
                </ThemeProvider>
                <div className="form">
                    {this.state.overlay}
                    <div className="sideBySide">
                        {mainLayer}
                        {this.printColoredXML()}
                    </div>
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

/**
 * Component representing a card with plugins of the same category
 */
class PluginTypeCard extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            anchorEl: null,
        };
    }

    /**
     * Adds a new plugin
     * @param {number} pluginIndex 
     */
    addPlugin(pluginIndex) {
        this.setState({ anchorEl: null });
        this.props.addPlugin(this.props.sectionIndex, pluginIndex);
    }

    /**
     * Removes an existing plugin
     * @param {number} index 
     */
    removePlugin(index) {
        this.props.removePlugin(this.props.sectionIndex, index);
    }

    /**
     * Edits an existing plugin
     * @param {number} index 
     */
    editPlugin(index) {
        this.props.editPlugin(this.props.sectionIndex, index);
    }

    /**
     * Handles a menu opening on add plugin button click
     * @param {*} event 
     */
    handleMenuClick(event) {
        this.setState({ anchorEl: event.currentTarget });
    }
    /**
     * Handles a menu closing
     */
    handleMenuClose() {
        this.setState({ anchorEl: null });
    }

    render() {
        let plugins = "";
        let errorIcon = "";
        if (this.props.errors.length > 0) {
            let errorMessage = this.props.errors[this.props.errors.length - 1].message;
            errorIcon = (
                <IconButton tabIndex={-1}>
                    <Tooltip title={errorMessage} arrow>
                        <Icon color={"error"}>error</Icon>
                    </Tooltip>
                </IconButton>
            );
        }
        if (this.props.plugins.length > 0) {
            plugins = this.props.plugins.map((plugin, index) => {
                return (
                    <CardContent key={index} className="plugin">
                        <Plugin
                            index={index}
                            plugin={plugin}
                            onRemove={this.removePlugin.bind(this)}
                            onEdit={this.editPlugin.bind(this)}
                        />
                    </CardContent>
                );
            });
        }
        let addMenu = (
            <React.Fragment>
                <ThemeProvider theme={GRAY_THEME}>
                    <Button
                        variant="outlined"
                        color="primary"
                        aria-controls="simple-menu"
                        aria-haspopup="true"
                        onClick={this.handleMenuClick.bind(this)}
                        color="primary"
                    >
                        Add plugin
                    </Button>
                </ThemeProvider>

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
                <CardHeader title={this.props.name} action={errorIcon} />
                <Divider />
                {plugins}
                <Divider />
                <CardActions disableSpacing>{addMenu}</CardActions>
            </Card>
        );
    }
}

/**
 * Component representing the menu item in menu opened on add plugin button click
 */
class PluginAvailable extends React.Component {
    /**
     * Handles a click action
     */
    handleAdd() {
        this.props.onAdd(this.props.pluginIndex);
    }

    render() {
        return <MenuItem onClick={this.handleAdd.bind(this)}>{this.props.plugin.title}</MenuItem>;
    }
}

/**
 * Component representing a plugin inside PluginTypeCard component
 */
class Plugin extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            detailVisible: false,
            expanded: false,
        };
    }

    /**
     * Handles plugin edit
     */
    handleEdit() {
        this.props.onEdit(this.props.index);
    }

    /**
     * Handles expand click
     */
    handleExpandClick() {
        this.setState({ expanded: !this.state.expanded });
    }

    render() {
        return (
            <Card>
                <CardHeader
                    action={
                        <React.Fragment>
                            <IconButton aria-label="edit" onClick={this.handleEdit.bind(this)}>
                                <Icon>edit</Icon>
                            </IconButton>
                            <IconButton aria-label="delete" onClick={this.props.onRemove}>
                                <Icon>delete</Icon>
                            </IconButton>
                            <IconButton
                                onClick={this.handleExpandClick.bind(this)}
                                aria-expanded={this.state.expanded}
                                aria-label="show more"
                            >
                                <Icon
                                    className={
                                        "expandable" + (this.state.expanded ? " expanded" : "")
                                    }
                                >
                                    expand_more
                                </Icon>
                            </IconButton>
                        </React.Fragment>
                    }
                    title={this.props.plugin.name}
                />
                <Collapse in={this.state.expanded} timeout="auto">
                    <CardContent>
                        <PropsItem plugin={this.props.plugin} />
                    </CardContent>
                </Collapse>
            </Card>
        );
    }
}

/**
 * Component representing a parameter print inside Plugin component
 */
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

/**
 * Component representing an overlay with settings
 */
class Settings extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            indentNumber: this.props.indentNumber,
            confirmDialodOpen: false,
        };
    }

    handleChangeType(event) {
        if (event.target.value === INDENTATION_TYPES[1].name) {
            this.props.onChange(INDENTATION_TYPES[1], 1, this.props.showConfirmationDialogs);
            this.setState({ indentNumber: 1 });
        } else if (this.props.indentType === INDENTATION_TYPES[1]) {
            this.props.onChange(INDENTATION_TYPES[0], 2, this.props.showConfirmationDialogs);
            this.setState({ indentNumber: 2 });
        } else {
            this.props.onChange(
                INDENTATION_TYPES[0],
                this.props.indentNumber,
                this.props.showConfirmationDialogs
            );
        }
    }

    handleChangeNumber(event) {
        let value = Number(event.target.value);
        if (value < INDENTATION_SPACES.min || value.toString() != event.target.value) {
            this.setState({ indentNumber: event.target.value });
            return;
        } else if (value > INDENTATION_SPACES.max) {
            value = INDENTATION_SPACES.max;
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
                            >
                                {INDENTATION_TYPES.map((indentType) => {
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
                                disabled={this.props.indentType === INDENTATION_TYPES[1]}
                                inputProps={{
                                    min: INDENTATION_SPACES.min,
                                    max: INDENTATION_SPACES.max,
                                    step: 1,
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
                                        color={"primary"}
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

// Obtain the root element
const rootAppElement = document.getElementById("configurator_app");

/**
 * Function starting the app
 */
function startApp() {
    ReactDOM.render(<App />, rootAppElement);
}

//App start call
startApp();
