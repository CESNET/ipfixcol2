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
    if (jsonSchema.hasOwnProperty("const")) {
        module[propertyName] = jsonSchema.const;
        return;
    }
    if (jsonSchema.hasOwnProperty("default")) {
        module[propertyName] = jsonSchema.default;
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
        var newArray = [];
        moduleArrayAddItem(newArray, jsonSchema.items);
        module[propertyName] = newArray;
        return;
    }
    module[propertyName] = null;
}

function moduleArrayAddItem(array, jsonSchema) {
    var item = null;
    if (jsonSchema.hasOwnProperty("const")) {
        item = jsonSchema.const;
    } else if (jsonSchema.hasOwnProperty("default")) {
        item = jsonSchema.default;
    } else if (jsonSchema.type === "object") {
        item = {};
        if (jsonSchema.hasOwnProperty("required")) {
            for (var i in jsonSchema.required) {
                moduleSetProperty(
                    item,
                    jsonSchema.required[i],
                    jsonSchema.properties[jsonSchema.required[i]]
                );
            }
        }
    } else if (jsonSchema.type === "array") {
        item = [];
        moduleArrayAddItem(item, jsonSchema.items);
    }
    array.push(item);
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
    handleComfirm() {
        if (this.state.isNew) {
            this.props.onSuccess(this.props.columnIndex, this.state.module);
        } else {
            this.props.onSuccess(this.props.columnIndex, this.props.index, this.state.module);
        }
    }
    handleChange(propertyName, changedSubmodule) {
        //TODO rework - 2 params => 1 param
        // var changedModule = changedSubmodule;
        // var changedModule = this.state.module;
        // changedModule[propertyName] = changedSubmodule;
        this.setState({
            module: changedSubmodule
        });
    }
    render() {
        var buttonText = this.state.isNew ? "Add module" : "Edit module";
        return (
            <Dialog
                disableBackdropClick
                disableEscapeKeyDown
                open={true}
                fullWidth={true}
                maxWidth={"md"}
            >
                <DialogTitle>{buttonText}</DialogTitle>
                <DialogContent>
                    <Grid container spacing={2}>
                        <Grid item md={6} sm={12} xs={12}>
                            <Properties
                                module={this.state.module}
                                jsonSchema={this.props.jsonSchema}
                                onChange={this.handleChange.bind(this)}
                                required={true}
                                isRoot={true}
                            />
                        </Grid>
                        <Grid item md={6} sm={12} xs={12}>
                            <FormControl fullWidth>
                                <TextField
                                    label={"Module XML"}
                                    multiline
                                    InputProps={{
                                        readOnly: true
                                    }}
                                    value={formatXml(x2js.json2xml_str(this.state.module))}
                                />
                                <FormHelperText>Read only</FormHelperText>
                            </FormControl>
                        </Grid>
                    </Grid>
                </DialogContent>
                <DialogActions>
                    <Button variant="outlined" color="primary" onClick={this.props.onCancel}>
                        Cancel
                    </Button>
                    <Button
                        variant="contained"
                        color="primary"
                        onClick={this.handleComfirm.bind(this)}
                    >
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
            anchorEl: null,
            expanded: true
        };
    }
    handleMenuClick(event) {
        this.setState({ anchorEl: event.currentTarget });
    }
    handleMenuClose() {
        this.setState({ anchorEl: null });
    }
    handleMenuSelect(selectedPropertyName) {
        var changedModule = JSON.parse(JSON.stringify(this.props.module));
        moduleSetProperty(
            changedModule,
            selectedPropertyName,
            this.props.jsonSchema.properties[selectedPropertyName]
        );
        this.handleMenuClose();
        this.props.onChange(this.props.name, changedModule);
        this.setState({ expanded: true });
    }
    handleChange(propertyName, changedSubmodule) {
        var changedModule = JSON.parse(JSON.stringify(this.props.module));
        changedModule[propertyName] = changedSubmodule;
        this.props.onChange(this.props.name, changedModule);
    }
    handleRemoveChild(propertyName) {
        var changedModule = JSON.parse(JSON.stringify(this.props.module));
        delete changedModule[propertyName];
        this.props.onChange(this.props.name, changedModule);
    }
    handleRemove() {
        this.props.onRemove(this.props.name);
    }
    handleExpandClick() {
        this.setState({ expanded: !this.state.expanded });
    }

    render() {
        var name = this.props.isRoot ? "" : this.props.name;
        var optionalMenu = "";
        var removeButton = "";
        var expandButton = "";
        var properties = "";
        if (
            (!this.props.jsonSchema.hasOwnProperty("required") ||
                Object.keys(this.props.jsonSchema.properties).length >
                    this.props.jsonSchema.required.length) &&
            Object.keys(this.props.module).length !=
                Object.keys(this.props.jsonSchema.properties).length
        ) {
            optionalMenu = (
                <div>
                    <Button
                        variant="outlined"
                        color="primary"
                        aria-controls="simple-menu"
                        aria-haspopup="true"
                        onClick={this.handleMenuClick.bind(this)}
                    >
                        Add optional parameter
                    </Button>
                    <Menu
                        id="simple-menu"
                        anchorEl={this.state.anchorEl}
                        keepMounted
                        open={Boolean(this.state.anchorEl)}
                        onClose={this.handleMenuClose.bind(this)}
                    >
                        {Object.keys(this.props.jsonSchema.properties).map(propertyName => {
                            if (
                                (!this.props.jsonSchema.hasOwnProperty("required") ||
                                    !this.props.jsonSchema.required.includes(propertyName)) &&
                                !this.props.module.hasOwnProperty(propertyName)
                            ) {
                                return (
                                    <MenuItem
                                        key={propertyName}
                                        onClick={this.handleMenuSelect.bind(this, propertyName)}
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
        if (this.props.hasOwnProperty("required") && !this.props.required) {
            removeButton = (
                <IconButton onClick={this.handleRemove.bind(this)}>
                    <Icon>delete</Icon>
                </IconButton>
            );
        }
        if (
            Object.keys(this.props.module).length !== 0 &&
            this.props.module.constructor === Object
        ) {
            expandButton = (
                <IconButton
                    className={"expandable" + (this.state.expanded ? " expanded" : "")}
                    onClick={this.handleExpandClick.bind(this)}
                    aria-expanded={this.state.expanded}
                    aria-label="show more"
                >
                    <Icon>expand_more</Icon>
                </IconButton>
            );
            properties = (
                <Collapse in={this.state.expanded} timeout="auto" unmountOnExit>
                    {Object.keys(this.props.jsonSchema.properties).map(propertyName => {
                        if (
                            (!this.props.jsonSchema.hasOwnProperty("required") ||
                                !this.props.jsonSchema.required.includes(propertyName)) &&
                            !this.props.module.hasOwnProperty(propertyName)
                        ) {
                            return;
                        }
                        var isOptional =
                            this.props.jsonSchema.hasOwnProperty("required") &&
                            this.props.jsonSchema.required.includes(propertyName);
                        return (
                            <CardContent key={propertyName}>
                                <Item
                                    name={propertyName}
                                    type={this.props.jsonSchema.properties[propertyName].type}
                                    module={this.props.module[propertyName]}
                                    required={isOptional}
                                    jsonSchema={this.props.jsonSchema.properties[propertyName]}
                                    onChange={this.handleChange.bind(this)}
                                    onRemove={this.handleRemoveChild.bind(this)}
                                />
                            </CardContent>
                        );
                    })}
                </Collapse>
            );
        }
        return (
            <Card>
                {name !== "" || removeButton !== "" ? (
                    <CardHeader
                        action={
                            <div>
                                {removeButton}
                                {expandButton}
                            </div>
                        }
                        title={name}
                    />
                ) : (
                    ""
                )}
                {properties}
                {optionalMenu !== "" ? (
                    <CardActions disableSpacing>{optionalMenu}</CardActions>
                ) : (
                    ""
                )}
            </Card>
        );
    }
}

class Item extends React.Component {
    render() {
        switch (this.props.type) {
            case "string":
                return (
                    <StringProperty
                        name={this.props.name}
                        module={this.props.module}
                        required={this.props.required}
                        jsonSchema={this.props.jsonSchema}
                        onChange={this.props.onChange}
                        onRemove={this.props.onRemove}
                    />
                );
            case "integer":
                return (
                    <IntegerProperty
                        name={this.props.name}
                        module={this.props.module}
                        required={this.props.required}
                        jsonSchema={this.props.jsonSchema}
                        onChange={this.props.onChange}
                        onRemove={this.props.onRemove}
                    />
                );
            case "object":
                return (
                    <Properties
                        name={this.props.name}
                        module={this.props.module}
                        required={this.props.required}
                        jsonSchema={this.props.jsonSchema}
                        onChange={this.props.onChange}
                        onRemove={this.props.onRemove}
                        isRoot={false}
                    />
                );
            case "boolean":
                return (
                    <BooleanProperty
                        name={this.props.name}
                        module={this.props.module}
                        required={this.props.required}
                        jsonSchema={this.props.jsonSchema}
                        onChange={this.props.onChange}
                        onRemove={this.props.onRemove}
                    />
                );
            case "number":
                return (
                    <NumberProperty
                        name={this.props.name}
                        module={this.props.module}
                        required={this.props.required}
                        jsonSchema={this.props.jsonSchema}
                        onChange={this.props.onChange}
                        onRemove={this.props.onRemove}
                    />
                );
            case "array":
                return (
                    <ArrayProperty
                        name={this.props.name}
                        module={this.props.module}
                        required={this.props.required}
                        jsonSchema={this.props.jsonSchema}
                        onChange={this.props.onChange}
                        onRemove={this.props.onRemove}
                    />
                );
            default:
                console.log("Unknown type: " + this.props.jsonSchema.properties[propertyName].type);
        }
    }
}

class ArrayProperty extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            expanded: true
        };
    }

    handleChange(index, _, changedSubmodule) {
        var changedModule = JSON.parse(JSON.stringify(this.props.module));
        changedModule[index] = changedSubmodule;
        this.props.onChange(this.props.name, changedModule);
    }
    handleRemove(index) {
        if (this.props.module.length > 1) {
            var changedModule = JSON.parse(JSON.stringify(this.props.module));
            changedModule.splice(index, 1);
            this.props.onChange(this.props.name, changedModule);
        } else {
            this.props.onRemove(this.props.name);
        }
    }
    handleAdd() {
        var changedModule = JSON.parse(JSON.stringify(this.props.module));
        moduleArrayAddItem(changedModule, this.props.jsonSchema.items);
        this.props.onChange(this.props.name, changedModule);
    }
    handleExpandClick() {
        this.setState({ expanded: !this.state.expanded });
    }

    render() {
        var name = this.props.name;
        var buttonText = "Add item";
        var button = "";
        var expandButton = "";
        var items = "";
        if (
            !this.props.jsonSchema.hasOwnProperty("maxItems") ||
            (this.props.jsonSchema.hasOwnProperty("maxItems") &&
                this.props.jsonSchema.maxItems > this.props.module.length)
        ) {
            button = (
                <Button variant="outlined" color="primary" onClick={this.handleAdd.bind(this)}>
                    {buttonText}
                </Button>
            );
        }
        expandButton = (
            <IconButton
                className={"expandable" + (this.state.expanded ? " expanded" : "")}
                onClick={this.handleExpandClick.bind(this)}
                aria-expanded={this.state.expanded}
                aria-label="show more"
            >
                <Icon>expand_more</Icon>
            </IconButton>
        );
        items = (
            <Collapse in={this.state.expanded} timeout="auto" unmountOnExit>
                {this.props.module.map((item, index) => {
                    return (
                        <CardContent key={index}>
                            <Item
                                name={"[" + index + "]"}
                                type={this.props.jsonSchema.items.type}
                                module={item}
                                required={false}
                                jsonSchema={this.props.jsonSchema.items}
                                onChange={this.handleChange.bind(this, index)}
                                onRemove={this.handleRemove.bind(this, index)}
                            />
                        </CardContent>
                    );
                })}
            </Collapse>
        );
        return (
            <Card>
                <CardHeader action={expandButton} title={name} subheader={"(Array)"} />
                {items}
                {button !== "" ? <CardActions disableSpacing>{button}</CardActions> : ""}
            </Card>
        );
    }
}

class StringProperty extends React.Component {
    handleChange(event) {
        this.props.onChange(this.props.name, event.target.value);
    }
    handleRemove() {
        this.props.onRemove(this.props.name);
    }
    render() {
        var value = this.props.module;
        var readOnly = false;
        var onChange = this.handleChange.bind(this);
        var inputCode;
        if (this.props.jsonSchema.hasOwnProperty("default") && value === null) {
            value = this.props.jsonSchema.default;
        }
        if (this.props.jsonSchema.hasOwnProperty("const")) {
            value = this.props.jsonSchema.const;
            readOnly = true;
            onChange = null;
        }
        if (value === null) {
            value = "";
        }
        if (this.props.jsonSchema.hasOwnProperty("enum")) {
            if (this.props.required) {
                inputCode = (
                    <FormControl>
                        <InputLabel>{this.props.name}</InputLabel>
                        <Select className={"select"} value={value} onChange={onChange} readOnly={readOnly}>
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
                    <FormControl>
                        <InputLabel>{this.props.name}</InputLabel>
                        <Select
                            className={"select"}
                            value={value}
                            onChange={onChange}
                            readOnly={readOnly}
                            endAdornment={
                                <InputAdornment position="end">
                                    <IconButton onClick={this.handleRemove.bind(this)}>
                                        <Icon>delete</Icon>
                                    </IconButton>
                                </InputAdornment>
                            }
                        >
                            {enumValues.map(enumValue => {
                                return (
                                    <MenuItem key={enumValue} value={enumValue}>
                                        {enumValue.toString()}
                                    </MenuItem>
                                );
                            })}
                        </Select>
                    </FormControl>
                );
            }
        } else if (!this.props.required) {
            inputCode = (
                <FormControl>
                    <InputLabel>{this.props.name}</InputLabel>
                    <Input
                        className={"select"}
                        type={"text"}
                        name={this.props.name}
                        value={value}
                        readOnly={readOnly}
                        required={this.props.required}
                        onChange={onChange}
                        endAdornment={
                            <InputAdornment position="end">
                                <IconButton onClick={this.handleRemove.bind(this)}>
                                    <Icon>delete</Icon>
                                </IconButton>
                            </InputAdornment>
                        }
                    />
                </FormControl>
            );
        } else {
            inputCode = (
                <TextField
                    className={"select"}
                    label={this.props.name}
                    type={"text"}
                    name={this.props.name}
                    value={value}
                    readOnly={readOnly}
                    required={this.props.required}
                    onChange={onChange}
                />
            );
        }
        return <div>{inputCode}</div>;
    }
}

class IntegerProperty extends React.Component {
    handleChange(event) {
        if (
            this.props.jsonSchema.hasOwnProperty("minimum") &&
            event.target.value < this.props.jsonSchema.minimum
        ) {
            this.props.onChange(this.props.name, this.props.jsonSchema.minimum);
        } else if (
            this.props.jsonSchema.hasOwnProperty("maximum") &&
            event.target.value > this.props.jsonSchema.maximum
        ) {
            this.props.onChange(this.props.name, this.props.jsonSchema.maximum);
        } else {
            this.props.onChange(this.props.name, event.target.value);
        }
    }
    handleRemove() {
        this.props.onRemove(this.props.name);
    }
    render() {
        var value = this.props.module;
        var readOnly = false;
        var min = null;
        var max = null;
        var inputCode;
        var onChange = this.handleChange.bind(this);
        if (this.props.jsonSchema.hasOwnProperty("default") && value === null) {
            value = this.props.jsonSchema.default;
        }
        if (this.props.jsonSchema.hasOwnProperty("const")) {
            value = this.props.jsonSchema.const;
            readOnly = true;
            onChange = null;
        }
        if (this.props.jsonSchema.hasOwnProperty("minimum")) {
            min = this.props.jsonSchema.minimum;
        }
        if (this.props.jsonSchema.hasOwnProperty("maximum")) {
            max = this.props.jsonSchema.maximum;
        }
        if (value === null) {
            value = "";
        }
        if (this.props.required) {
            inputCode = (
                <TextField
                    className={"select"}
                    label={this.props.name}
                    type={"number"}
                    name={this.props.name}
                    value={value}
                    readOnly={readOnly}
                    required={this.props.required}
                    inputProps={{ min: min, max: max, step: 1 }}
                    onChange={onChange}
                />
            );
        } else {
            inputCode = (
                <FormControl>
                    <InputLabel>{this.props.name}</InputLabel>
                    <Input
                        className={"select"}
                        type={"number"}
                        name={this.props.name}
                        value={value}
                        readOnly={readOnly}
                        required={this.props.required}
                        inputProps={{ min: min, max: max, step: 1 }}
                        onChange={onChange}
                        endAdornment={
                            <InputAdornment position="end">
                                <IconButton onClick={this.handleRemove.bind(this)}>
                                    <Icon>delete</Icon>
                                </IconButton>
                            </InputAdornment>
                        }
                    />
                </FormControl>
            );
        }
        return <div>{inputCode}</div>;
    }
}

class BooleanProperty extends React.Component {
    handleChange(event) {
        this.props.onChange(this.props.name, event.target.value);
    }
    handleRemove() {
        this.props.onRemove(this.props.name);
    }
    render() {
        var value = this.props.module;
        var readOnly = false;
        var inputCode;
        var onChange = this.handleChange.bind(this);
        if (this.props.jsonSchema.hasOwnProperty("default") && value === undefined) {
            value = this.props.jsonSchema.default;
        }
        if (this.props.jsonSchema.hasOwnProperty("const")) {
            value = this.props.jsonSchema.const;
            readOnly = true;
            onChange = null;
        }
        if (value === null) {
            value = "";
        }
        var enumValues = [true, false];
        if (this.props.jsonSchema.hasOwnProperty("enum")) {
            enumValues = this.props.jsonSchema.enum;
        }
        if (this.props.required) {
            inputCode = (
                <FormControl>
                    <InputLabel>{this.props.name}</InputLabel>
                    <Select className={"select"} value={value} onChange={onChange} readOnly={readOnly}>
                        {enumValues.map(enumValue => {
                            return (
                                <MenuItem key={enumValue} value={enumValue}>
                                    {enumValue.toString()}
                                </MenuItem>
                            );
                        })}
                    </Select>
                </FormControl>
            );
        } else {
            inputCode = (
                <FormControl>
                    <InputLabel>{this.props.name}</InputLabel>
                    <Select
                        className={"select"}
                        value={value}
                        onChange={onChange}
                        readOnly={readOnly}
                        endAdornment={
                            <InputAdornment position="end">
                                <IconButton onClick={this.handleRemove.bind(this)}>
                                    <Icon>delete</Icon>
                                </IconButton>
                            </InputAdornment>
                        }
                    >
                        {enumValues.map(enumValue => {
                            return (
                                <MenuItem key={enumValue} value={enumValue}>
                                    {enumValue.toString()}
                                </MenuItem>
                            );
                        })}
                    </Select>
                </FormControl>
            );
        }
        return <div>{inputCode}</div>;
    }
}

class NumberProperty extends React.Component {
    handleChange(event) {
        if (
            this.props.jsonSchema.hasOwnProperty("minimum") &&
            event.target.value < this.props.jsonSchema.minimum
        ) {
            this.props.onChange(this.props.name, this.props.jsonSchema.minimum);
        } else if (
            this.props.jsonSchema.hasOwnProperty("maximum") &&
            event.target.value > this.props.jsonSchema.maximum
        ) {
            this.props.onChange(this.props.name, this.props.jsonSchema.maximum);
        } else {
            this.props.onChange(this.props.name, event.target.value);
        }
    }
    handleRemove() {
        this.props.onRemove(this.props.name);
    }
    render() {
        var value = this.props.module;
        var readOnly = false;
        var min = null;
        var max = null;
        var inputCode;
        var onChange = this.handleChange.bind(this);
        if (this.props.jsonSchema.hasOwnProperty("default") && value === null) {
            value = this.props.jsonSchema.default;
        }
        if (this.props.jsonSchema.hasOwnProperty("const")) {
            value = this.props.jsonSchema.const;
            readOnly = true;
            onChange = null;
        }
        if (this.props.jsonSchema.hasOwnProperty("minimum")) {
            min = this.props.jsonSchema.minimum;
        }
        if (this.props.jsonSchema.hasOwnProperty("maximum")) {
            max = this.props.jsonSchema.maximum;
        }
        if (value === null) {
            value = "";
        }
        if (this.props.required) {
            <TextField
                className={"select"}
                label={this.props.name}
                type={"number"}
                name={this.props.name}
                value={value}
                readOnly={readOnly}
                required={this.props.required}
                inputProps={{ min: min, max: max, step: 0.01 }}
                onChange={onChange}
            />;
        } else {
            inputCode = (
                <FormControl>
                    <InputLabel>{this.props.name}</InputLabel>
                    <Input
                        className={"select"}
                        type={"number"}
                        name={this.props.name}
                        value={value}
                        readOnly={readOnly}
                        required={this.props.required}
                        inputProps={{ min: min, max: max, step: 1 }}
                        onChange={onChange}
                        endAdornment={
                            <InputAdornment position="end">
                                <IconButton onClick={this.handleRemove.bind(this)}>
                                    <Icon>delete</Icon>
                                </IconButton>
                            </InputAdornment>
                        }
                    />
                </FormControl>
            );
        }
        return <div>{inputCode}</div>;
    }
}
