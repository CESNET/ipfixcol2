function moduleCreate(jsonSchema) {
    var newModule = {};
    for (var i in jsonSchema.required) {
        moduleSetProperty(
            newModule,
            jsonSchema.required[i],
            jsonSchema.properties[jsonSchema.required[i]]
        );
    }
    return newModule;
}

function moduleSetProperty(module, propertyName, jsonSchema) {
    if (jsonSchema.hasOwnProperty("default")) {
        module[propertyName] = jsonSchema.default;
        return;
    }
    if (jsonSchema.hasOwnProperty("const")) {
        module[propertyName] = jsonSchema.const;
        return;
    }
    if (jsonSchema.type === "object") {
        module[propertyName] = {};
        if (jsonSchema.hasOwnProperty("required")) {
            for (var i in jsonSchema.required) {
                moduleSetProperty(
                    module[propertyName],
                    jsonSchema.required[i],
                    jsonSchema.properties[jsonSchema.required[i]]
                );
            }
        }
        return;
    }
    if (jsonSchema.type === "array") {
        module[propertyName] = [];
        return;
    }
    module[propertyName] = null;
}

class Overlay extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            module:
                this.props.module !== undefined
                    ? this.props.module
                    : moduleCreate(this.props.jsonSchema),
            isNew: this.props.module === undefined
        };
    }
    render() {
        var buttonText = this.state.isNew ? "Add module" : "Edit module";
        return (
            <Dialog disableBackdropClick disableEscapeKeyDown open={true} fullWidth={true} maxWidth={"md"}>
                <DialogTitle>{buttonText}</DialogTitle>
                <DialogContent>
                    <Grid container spacing={2}>
                        <Grid item md={6}>
                            <Properties jsonSchema={this.props.jsonSchema} isRoot={true} />
                        </Grid>
                        <Grid item md={6}>
                            <TextareaAutosize defaultValue={formatXml(x2js.json2xml_str(this.state.module))} />
                        </Grid>
                    </Grid>
                </DialogContent>
                <DialogActions>
                    <Button variant="outlined" color="primary" onClick={this.props.onCancel}>
                        Cancel
                    </Button>
                    <Button variant="contained" color="primary">
                        {buttonText}
                    </Button>
                </DialogActions>
            </Dialog>
        );
    }
}

class Properties extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            anchorEl: null
        };
    }
    handleClick = event => {
        this.setState({ anchorEl: event.currentTarget });
    };
    handleClose = () => {
        this.setState({ anchorEl: null });
    };

    render() {
        var className = this.props.isRoot ? "rootProps" : "innerProps";
        var name = this.props.isRoot ? "" : this.props.name;
        var optional = "";
        if (
            !this.props.jsonSchema.hasOwnProperty("required") ||
            Object.keys(this.props.jsonSchema.properties).length >
                this.props.jsonSchema.required.length
        ) {
            optional = (
                <div>
                    <Button
                        variant="contained"
                        color="primary"
                        aria-controls="simple-menu"
                        aria-haspopup="true"
                        onClick={this.handleClick.bind(this)}
                    >
                        Add optional parameter
                    </Button>
                    <Menu
                        id="simple-menu"
                        anchorEl={this.state.anchorEl}
                        keepMounted
                        open={Boolean(this.state.anchorEl)}
                        onClose={this.handleClose.bind(this)}
                    >
                        {Object.keys(this.props.jsonSchema.properties).map(propertyName => {
                            if (
                                !this.props.jsonSchema.hasOwnProperty("required") ||
                                !this.props.jsonSchema.required.includes(propertyName)
                            ) {
                                return (
                                    <MenuItem
                                        key={propertyName}
                                        onClick={this.handleClose.bind(this)}
                                    >
                                        {propertyName}
                                    </MenuItem>
                                );
                            }
                        })}
                    </Menu>
                </div>
            );
        }
        return (
            <div className={className}>
                <p>{name}</p>
                {Object.keys(this.props.jsonSchema.properties).map(propertyName => {
                    if (
                        !this.props.jsonSchema.hasOwnProperty("required") ||
                        !this.props.jsonSchema.required.includes(propertyName)
                    ) {
                        return;
                    }
                    switch (this.props.jsonSchema.properties[propertyName].type) {
                        case "string":
                            return (
                                <StringProperty
                                    key={propertyName}
                                    name={propertyName}
                                    required={true}
                                    jsonSchema={this.props.jsonSchema.properties[propertyName]}
                                />
                            );
                        case "integer":
                            return (
                                <IntegerProperty
                                    key={propertyName}
                                    name={propertyName}
                                    required={true}
                                    jsonSchema={this.props.jsonSchema.properties[propertyName]}
                                />
                            );
                        case "object":
                            return (
                                <Properties
                                    key={propertyName}
                                    name={propertyName}
                                    required={true}
                                    isRoot={false}
                                    jsonSchema={this.props.jsonSchema.properties[propertyName]}
                                />
                            );
                        case "boolean":
                            return (
                                <BooleanProperty
                                    key={propertyName}
                                    name={propertyName}
                                    required={true}
                                    jsonSchema={this.props.jsonSchema.properties[propertyName]}
                                />
                            );
                        case "number":
                            return (
                                <NumberProperty
                                    key={propertyName}
                                    name={propertyName}
                                    required={true}
                                    jsonSchema={this.props.jsonSchema.properties[propertyName]}
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
        var inputCode;
        if (this.props.jsonSchema.hasOwnProperty("default")) {
            value = this.props.jsonSchema.default;
        }
        if (this.props.jsonSchema.hasOwnProperty("const")) {
            value = this.props.jsonSchema.const;
            readOnly = true;
        }
        if (this.props.jsonSchema.hasOwnProperty("enum")) {
            inputCode = (
                <FormControl className={"select"}>
                    <InputLabel>{this.props.name}</InputLabel>
                    <Select value={value}>
                        {this.props.jsonSchema.enum.map(enumValue => {
                            return (
                                <MenuItem key={enumValue} value={enumValue}>
                                    {enumValue}
                                </MenuItem>
                            );
                        })}
                    </Select>
                </FormControl>
            );
        } else {
            inputCode = (
                <TextField
                    label={this.props.name}
                    type={"text"}
                    name={this.props.name}
                    value={value}
                    readOnly={readOnly}
                    required={this.props.required}
                />
            );
        }
        return <div>{inputCode}</div>;
    }
}

class IntegerProperty extends React.Component {
    render() {
        var value = "";
        var readOnly = false;
        var min = null;
        var max = null;
        if (this.props.jsonSchema.hasOwnProperty("default")) {
            value = this.props.jsonSchema.default;
        }
        if (this.props.jsonSchema.hasOwnProperty("const")) {
            value = this.props.jsonSchema.const;
            readOnly = true;
        }
        if (this.props.jsonSchema.hasOwnProperty("minimum")) {
            min = this.props.jsonSchema.minimum;
        }
        if (this.props.jsonSchema.hasOwnProperty("maximum")) {
            max = this.props.jsonSchema.maximum;
        }
        return (
            <TextField
                label={this.props.name}
                type={"number"}
                step={1}
                name={this.props.name}
                value={value}
                readOnly={readOnly}
                required={this.props.required}
                min={min}
                max={max}
            />
        );
    }
}

class BooleanProperty extends React.Component {
    render() {
        var value = true;
        var readOnly = false;
        var inputCode;
        if (this.props.jsonSchema.hasOwnProperty("default")) {
            value = this.props.jsonSchema.default;
        }
        if (this.props.jsonSchema.hasOwnProperty("const")) {
            value = this.props.jsonSchema.const;
            readOnly = true;
        }
        var enumValues = [true, false];
        if (this.props.jsonSchema.hasOwnProperty("enum")) {
            enumValues = this.props.jsonSchema.enum;
        }
        return (
            <div>
                <FormControl className={"select"}>
                    <InputLabel>{this.props.name}</InputLabel>
                    <Select value={value}>
                        {this.props.jsonSchema.enum.map(enumValue => {
                            return (
                                <MenuItem key={enumValue} value={enumValue}>
                                    {enumValue.toString()}
                                </MenuItem>
                            );
                        })}
                    </Select>
                </FormControl>
            </div>
        );
    }
}

class NumberProperty extends React.Component {
    render() {
        var value = 0.0;
        var readOnly = false;
        var min = null;
        var max = null;
        if (this.props.jsonSchema.hasOwnProperty("default")) {
            value = this.props.jsonSchema.default;
        }
        if (this.props.jsonSchema.hasOwnProperty("const")) {
            value = this.props.jsonSchema.const;
            readOnly = true;
        }
        if (this.props.jsonSchema.hasOwnProperty("minimum")) {
            min = this.props.jsonSchema.minimum;
        }
        if (this.props.jsonSchema.hasOwnProperty("maximum")) {
            max = this.props.jsonSchema.maximum;
        }
        return (
            <TextField
                label={this.props.name}
                type={"number"}
                step={0.01}
                name={this.props.name}
                value={value}
                readOnly={readOnly}
                required={this.props.required}
                min={min}
                max={max}
            />
        );
    }
}
