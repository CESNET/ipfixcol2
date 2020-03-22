// TODO
// - Zredukovat opakování kódu
// - Přidat nové pluginy
// - rezdělit schémata do souborů
// - Plugin UniRec (timeout) přidat našeptávač možných hodnot
// - trochu vylepšit styl výpisu
// ? overlay - menší padding nebo zajistit, aby se ikony za vstupními poli nezalamovaly na nový řádek
//
// +- tabindex - tlačítka Cancel a Add/Edit module tabIndex zatím nemají
// +? musí být možnost zadat více IP adres
//      - není jasné, zda v případě více IP adres může být jedna z nich prázdná
//      - změna by kromě větší změny ve schématu znamenala přepsání velké části třídy ArrayProperty
// +? Předělat hlavní výpis modulů ve sloupcích
//      - zda je to vůbec potřeba, jestli nestačí jenom název, edit, delete
//
// + XML výpis při editaci modulů nerespektuje nastavení odsaszení
// + opravit pole pro zadávání číselných hodnot (formulář nerespektuje nastavené meze)
// + oprava validace IP adres
// + overlay - přidat křížek s funkcí cancel (v případě změny se dotázat, zda to chce uživatel opravdu udělat)

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
    if (jsonSchema.type === "boolean") {
        module[propertyName] = false;
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
        var module =
            this.props.module !== undefined
                ? this.props.module
                : moduleCreate(this.props.jsonSchema);
        var valid = ajv.validate(this.props.jsonSchema, module);
        var errors = undefined;
        console.log("valid: " + valid);
        if (!valid) {
            console.log(ajv.errors);
            errors = JSON.parse(JSON.stringify(ajv.errors));
        }
        this.state = {
            module: module,
            isNew: this.props.module === undefined,
            errors: errors,
            saveDialogOpen: false,
            confirmDialodOpen: false
        };
    }
    handleCancel() {
        if (
            !this.state.isNew &&
            JSON.stringify(this.state.module) == JSON.stringify(this.props.module)
        ) {
            this.props.onCancel();
        } else if (this.state.errors === undefined) {
            this.setState({ saveDialogOpen: true });
        } else {
            this.setState({ confirmDialodOpen: true });
        }
    }
    handleSaveDialogClose(saved) {
        this.setState({ saveDialogOpen: false });
        if (saved) {
            this.handleComfirm();
        } else {
            this.props.onCancel();
        }
    }
    handleComfirmDialogClose(confirmed) {
        this.setState({ confirmDialodOpen: false });
        if (confirmed) {
            this.props.onCancel();
        }
    }
    handleComfirm() {
        if (this.state.isNew) {
            this.props.onSuccess(this.props.columnIndex, this.state.module);
        } else {
            this.props.onSuccess(this.props.columnIndex, this.props.index, this.state.module);
        }
    }
    handleChange(changedSubmodule) {
        var valid = ajv.validate(this.props.jsonSchema, changedSubmodule);
        var errors = undefined;
        console.log("valid: " + valid);
        if (!valid) {
            console.log(ajv.errors);
            errors = JSON.parse(JSON.stringify(ajv.errors));
        }
        this.setState({
            module: changedSubmodule,
            errors: errors
        });
    }
    render() {
        var buttonText = this.state.isNew ? "Add module" : "Edit module";
        var titleText = buttonText + ": " + this.props.jsonSchema.title;
        var descParts = this.props.jsonSchema.description.split("|");
        var tabIndex = { counter: 0 };
        var subtitleText;
        var link;
        var properties;
        var btnCancel;
        var btnSave;
        if (descParts.length === 1 || descParts.length > 2) {
            subtitleText = this.props.jsonSchema.description;
        } else {
            subtitleText = descParts[0];
            link = (
                <Tooltip
                    title={this.props.jsonSchema.title + " GitHub page"}
                    arrow
                    placement={"bottom"}
                >
                    <IconButton
                        className={"overlayIcon"}
                        color={"inherit"}
                        target={"_blank"}
                        href={descParts[1]}
                    >
                        <SvgIcon>
                            <path d="M12.007 0C6.12 0 1.1 4.27.157 10.08c-.944 5.813 2.468 11.45 8.054 13.312.19.064.397.033.555-.084.16-.117.25-.304.244-.5v-2.042c-3.33.735-4.037-1.56-4.037-1.56-.22-.726-.694-1.35-1.334-1.756-1.096-.75.074-.735.074-.735.773.103 1.454.557 1.846 1.23.694 1.21 2.23 1.638 3.45.96.056-.61.327-1.178.766-1.605-2.67-.3-5.462-1.335-5.462-6.002-.02-1.193.42-2.35 1.23-3.226-.327-1.015-.27-2.116.166-3.09 0 0 1.006-.33 3.3 1.23 1.966-.538 4.04-.538 6.003 0 2.295-1.5 3.3-1.23 3.3-1.23.445 1.006.49 2.144.12 3.18.81.877 1.25 2.033 1.23 3.226 0 4.607-2.805 5.627-5.476 5.927.578.583.88 1.386.825 2.206v3.29c-.005.2.092.393.26.507.164.115.377.14.565.063 5.568-1.88 8.956-7.514 8.007-13.313C22.892 4.267 17.884.007 12.008 0z" />
                        </SvgIcon>
                    </IconButton>
                </Tooltip>
            );
        }
        properties = (
            <Properties
                module={this.state.module}
                jsonSchema={this.props.jsonSchema}
                errors={this.state.errors}
                dataPath={""}
                onChange={this.handleChange.bind(this)}
                required={true}
                isRoot={true}
                tabIndex={tabIndex}
            />
        );
        tabIndex.counter += 1;
        btnCancel = (
            <Button variant="outlined" color="primary" onClick={this.props.onCancel} tabIndex={0}>
                Cancel
            </Button>
        );
        tabIndex.counter += 1;
        btnSave = (
            <Button
                variant="contained"
                color="primary"
                onClick={this.handleComfirm.bind(this)}
                disabled={this.state.errors === undefined ? false : true}
                tabIndex={0}
            >
                {buttonText}
            </Button>
        );
        return (
            <Dialog
                disableBackdropClick
                disableEscapeKeyDown
                open={true}
                fullWidth={true}
                maxWidth={"md"}
            >
                <DialogTitle>
                    {titleText}
                    <div className={"overlaySubtitle"}>{subtitleText}</div>
                    {link}
                    <IconButton
                        className={"overlayIcon"}
                        color={"inherit"}
                        onClick={this.handleCancel.bind(this)}
                    >
                        <Icon>close</Icon>
                    </IconButton>
                </DialogTitle>
                <Divider />
                <DialogContent dividers>
                    <Grid container spacing={2}>
                        <Grid item md={6} sm={12} xs={12}>
                            {properties}
                        </Grid>
                        <Grid item md={6} sm={12} xs={12}>
                            <FormControl fullWidth>
                                <Typography
                                    id={"XMLPrint-title"}
                                    variant={"subtitle1"}
                                    component={"label"}
                                    color={"textSecondary"}
                                >
                                    Module XML
                                </Typography>
                                <Typography id={"XMLPrint"} component={"pre"}>
                                    {formatXml(
                                        x2js.json2xml_str(this.state.module),
                                        this.props.XMLIndentType.character,
                                        this.props.XMLIndentNumber
                                    )}
                                </Typography>
                                <Divider />
                                <FormHelperText>Read only</FormHelperText>
                            </FormControl>
                        </Grid>
                    </Grid>
                </DialogContent>
                <DialogActions>
                    {btnCancel}
                    {btnSave}
                </DialogActions>
                <Dialog
                    className={"settings"}
                    open={this.state.saveDialogOpen}
                    fullWidth={false}
                    maxWidth={"sm"}
                >
                    <DialogTitle>{"Save changes?"}</DialogTitle>
                    <Divider />
                    <DialogContent dividers>
                        <Typography>Do you want to save your changes?</Typography>
                    </DialogContent>
                    <DialogActions>
                        <Button
                            autoFocus
                            color="primary"
                            onClick={this.handleSaveDialogClose.bind(this, true)}
                        >
                            {"Yes"}
                        </Button>
                        <Button
                            color="primary"
                            onClick={this.handleSaveDialogClose.bind(this, false)}
                        >
                            {"No"}
                        </Button>
                    </DialogActions>
                </Dialog>
                <Dialog
                    className={"settings"}
                    open={this.state.confirmDialodOpen}
                    fullWidth={false}
                    maxWidth={"sm"}
                >
                    <DialogTitle>{"Are you sure?"}</DialogTitle>
                    <Divider />
                    <DialogContent dividers>
                        <Typography>All your changes will be lost.</Typography>
                        <Typography>Do you want to proceed?</Typography>
                    </DialogContent>
                    <DialogActions>
                        <Button
                            autoFocus
                            color="primary"
                            onClick={this.handleComfirmDialogClose.bind(this, false)}
                        >
                            {"Cancel"}
                        </Button>
                        <Button
                            color="primary"
                            onClick={this.handleComfirmDialogClose.bind(this, true)}
                        >
                            {"Continue"}
                        </Button>
                    </DialogActions>
                </Dialog>
            </Dialog>
        );
    }
}

class Properties extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            anchorEl: null,
            expanded: true,
            descriptionOpen: false
        };
    }
    handleMenuClick(event) {
        this.setState({ anchorEl: event.currentTarget });
    }
    handleMenuClose() {
        this.setState({ anchorEl: null });
    }
    callOnChange(changedModule) {
        if (this.props.isRoot) {
            this.props.onChange(changedModule);
            return;
        }
        this.props.onChange(this.props.name, changedModule);
    }
    handleMenuSelect(selectedPropertyName) {
        var changedModule = JSON.parse(JSON.stringify(this.props.module));
        moduleSetProperty(
            changedModule,
            selectedPropertyName,
            this.props.jsonSchema.properties[selectedPropertyName]
        );
        this.handleMenuClose();
        this.callOnChange(changedModule);
        this.setState({ expanded: true });
    }
    handleChange(propertyName, changedSubmodule) {
        var changedModule = JSON.parse(JSON.stringify(this.props.module));
        changedModule[propertyName] = changedSubmodule;
        this.callOnChange(changedModule);
    }
    handleRemoveChild(propertyName) {
        var changedModule = JSON.parse(JSON.stringify(this.props.module));
        delete changedModule[propertyName];
        this.callOnChange(changedModule);
    }
    handleRemove() {
        this.props.onRemove(this.props.name);
    }
    handleExpandClick() {
        this.setState({ expanded: !this.state.expanded });
    }
    handleDescriptionOpen() {
        this.setState({
            descriptionOpen: true
        });
    }
    handleDescriptionClose() {
        this.setState({
            descriptionOpen: false
        });
    }

    render() {
        var name = this.props.isRoot ? "" : this.props.jsonSchema.title;
        var optionalMenu = "";
        var errorIcon = "";
        var removeButton = "";
        var expandButton = "";
        var properties = "";
        var hasError = false;
        var propsErrors;
        var childErrorsNum = 0;
        var errorMessage = "";
        if (
            (!this.props.jsonSchema.hasOwnProperty("required") ||
                Object.keys(this.props.jsonSchema.properties).length >
                    this.props.jsonSchema.required.length) &&
            Object.keys(this.props.module).length !=
                Object.keys(this.props.jsonSchema.properties).length
        ) {
            optionalMenu = (
                <React.Fragment>
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
                                        {this.props.jsonSchema.properties[propertyName].title}
                                    </MenuItem>
                                );
                            }
                        })}
                    </Menu>
                </React.Fragment>
            );
        }
        if (this.props.errors !== undefined) {
            propsErrors = Object.values(this.props.errors).filter(error => {
                return error.dataPath == this.props.dataPath;
            });
            hasError = propsErrors.length > 0;
            childErrorsNum = this.props.errors.length - propsErrors.length;
        }
        if (hasError) {
            errorMessage = propsErrors.pop().message;
            errorIcon = (
                <IconButton>
                    <Tooltip title={errorMessage} arrow>
                        <Icon color={"error"}>error</Icon>
                    </Tooltip>
                </IconButton>
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
                    onClick={this.handleExpandClick.bind(this)}
                    aria-expanded={this.state.expanded}
                    aria-label="show more"
                >
                    <Badge
                        color={"error"}
                        badgeContent={childErrorsNum}
                        invisible={this.state.expanded || childErrorsNum == 0}
                    >
                        <Icon className={"expandable" + (this.state.expanded ? " expanded" : "")}>
                            expand_more
                        </Icon>
                    </Badge>
                </IconButton>
            );
            properties = (
                <Collapse in={this.state.expanded} timeout="auto">
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
                        var dataPath = this.props.dataPath + "." + propertyName;
                        var errors = undefined;
                        if (this.props.errors !== undefined) {
                            errors = Object.values(this.props.errors).filter(error => {
                                if (
                                    error.dataPath == dataPath ||
                                    error.dataPath.startsWith(dataPath + ".") ||
                                    error.dataPath.startsWith(dataPath + "[")
                                ) {
                                    return true;
                                }
                                return false;
                            });
                            if (errors === []) {
                                errors = undefined;
                            }
                        }
                        return (
                            <CardContent key={propertyName}>
                                <Item
                                    name={propertyName}
                                    type={this.props.jsonSchema.properties[propertyName].type}
                                    module={this.props.module[propertyName]}
                                    required={isOptional}
                                    jsonSchema={this.props.jsonSchema.properties[propertyName]}
                                    errors={errors}
                                    dataPath={dataPath}
                                    onChange={this.handleChange.bind(this)}
                                    onRemove={this.handleRemoveChild.bind(this)}
                                    tabIndex={this.props.tabIndex}
                                />
                            </CardContent>
                        );
                    })}
                </Collapse>
            );
        }
        return Object.keys(this.props.jsonSchema.properties).length == 0 ? (
            <CardContent>{"No available parameters"}</CardContent>
        ) : (
            <React.Fragment>
                <Card>
                    {name !== "" || removeButton !== "" ? (
                        <CardHeader
                            action={
                                <React.Fragment>
                                    {errorIcon}
                                    {removeButton}
                                    <IconButton onClick={this.handleDescriptionOpen.bind(this)}>
                                        <Icon>help</Icon>
                                    </IconButton>
                                    {expandButton}
                                </React.Fragment>
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
                <Description
                    open={this.state.descriptionOpen}
                    content={this.props.jsonSchema.description}
                    onClose={this.handleDescriptionClose.bind(this)}
                />
            </React.Fragment>
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
                        errors={this.props.errors}
                        dataPath={this.props.dataPath}
                        onChange={this.props.onChange}
                        onRemove={this.props.onRemove}
                        tabIndex={this.props.tabIndex}
                    />
                );
            case "integer":
                return (
                    <IntegerProperty
                        name={this.props.name}
                        module={this.props.module}
                        required={this.props.required}
                        jsonSchema={this.props.jsonSchema}
                        errors={this.props.errors}
                        dataPath={this.props.dataPath}
                        onChange={this.props.onChange}
                        onRemove={this.props.onRemove}
                        tabIndex={this.props.tabIndex}
                    />
                );
            case "object":
                return (
                    <Properties
                        name={this.props.name}
                        module={this.props.module}
                        required={this.props.required}
                        jsonSchema={this.props.jsonSchema}
                        errors={this.props.errors}
                        dataPath={this.props.dataPath}
                        onChange={this.props.onChange}
                        onRemove={this.props.onRemove}
                        isRoot={false}
                        tabIndex={this.props.tabIndex}
                    />
                );
            case "boolean":
                return (
                    <BooleanProperty
                        name={this.props.name}
                        module={this.props.module}
                        required={this.props.required}
                        jsonSchema={this.props.jsonSchema}
                        errors={this.props.errors}
                        dataPath={this.props.dataPath}
                        onChange={this.props.onChange}
                        onRemove={this.props.onRemove}
                        tabIndex={this.props.tabIndex}
                    />
                );
            case "number":
                return (
                    <NumberProperty
                        name={this.props.name}
                        module={this.props.module}
                        required={this.props.required}
                        jsonSchema={this.props.jsonSchema}
                        errors={this.props.errors}
                        dataPath={this.props.dataPath}
                        onChange={this.props.onChange}
                        onRemove={this.props.onRemove}
                        tabIndex={this.props.tabIndex}
                    />
                );
            case "array":
                return (
                    <ArrayProperty
                        name={this.props.name}
                        module={this.props.module}
                        required={this.props.required}
                        jsonSchema={this.props.jsonSchema}
                        errors={this.props.errors}
                        dataPath={this.props.dataPath}
                        onChange={this.props.onChange}
                        onRemove={this.props.onRemove}
                        tabIndex={this.props.tabIndex}
                    />
                );
            default:
                if (typeof this.props.type === typeof []) {
                    console.log("Multiple types");
                    return (
                        <MultipleTypesProperty
                            name={this.props.name}
                            module={this.props.module}
                            required={this.props.required}
                            jsonSchema={this.props.jsonSchema}
                            errors={this.props.errors}
                            dataPath={this.props.dataPath}
                            onChange={this.props.onChange}
                            onRemove={this.props.onRemove}
                            tabIndex={this.props.tabIndex}
                        />
                    );
                } else {
                    console.log(
                        "Unknown type: " + this.props.jsonSchema.properties[propertyName].type
                    );
                }
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
        this.setState({ expanded: true });
    }
    handleExpandClick() {
        this.setState({ expanded: !this.state.expanded });
    }

    render() {
        var name = this.props.jsonSchema.title;
        var buttonText = "Add item";
        var button = "";
        var errorIcon = "";
        var expandButton = "";
        var items = "";
        var hasError = false;
        var propsErrors;
        var childErrorsNum = 0;
        var errorMessage = "";
        var minItems = 0;
        var numOfItems = this.props.module.length;
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
        if (this.props.jsonSchema.hasOwnProperty("minItems")) {
            minItems = this.props.jsonSchema.minItems;
        }
        if (this.props.errors !== undefined) {
            propsErrors = Object.values(this.props.errors).filter(error => {
                if (error.dataPath == this.props.dataPath) {
                    return true;
                }
                return false;
            });
            hasError = propsErrors.length > 0;
            childErrorsNum = this.props.errors.length - propsErrors.length;
        }
        if (hasError) {
            errorMessage = propsErrors.pop().message;
            errorIcon = (
                <IconButton>
                    <Tooltip title={errorMessage} arrow>
                        <Icon color={"error"}>error</Icon>
                    </Tooltip>
                </IconButton>
            );
        }
        expandButton = (
            <IconButton
                onClick={this.handleExpandClick.bind(this)}
                aria-expanded={this.state.expanded}
                aria-label="show more"
            >
                <Badge
                    color={"error"}
                    badgeContent={childErrorsNum}
                    invisible={this.state.expanded || childErrorsNum == 0}
                >
                    <Icon className={"expandable" + (this.state.expanded ? " expanded" : "")}>
                        expand_more
                    </Icon>
                </Badge>
            </IconButton>
        );
        items = (
            <Collapse in={this.state.expanded} timeout="auto">
                {this.props.module.map((item, index) => {
                    var dataPath = this.props.dataPath + "[" + index + "]";
                    var errors = undefined;
                    if (this.props.errors !== undefined) {
                        errors = Object.values(this.props.errors).filter(error => {
                            if (
                                error.dataPath == dataPath ||
                                error.dataPath.startsWith(dataPath + ".") ||
                                error.dataPath.startsWith(dataPath + "[")
                            ) {
                                return true;
                            }
                            return false;
                        });
                        if (errors === []) {
                            errors = undefined;
                        }
                    }
                    return (
                        <CardContent key={index}>
                            <Item
                                name={this.props.name + "[" + index + "]"}
                                type={this.props.jsonSchema.items.type}
                                module={item}
                                required={minItems >= numOfItems && this.props.required}
                                jsonSchema={this.props.jsonSchema.items}
                                errors={errors}
                                dataPath={dataPath}
                                onChange={this.handleChange.bind(this, index)}
                                onRemove={this.handleRemove.bind(this, index)}
                                tabIndex={this.props.tabIndex}
                            />
                        </CardContent>
                    );
                })}
            </Collapse>
        );
        return (
            <Card>
                <CardHeader
                    action={
                        <div>
                            {errorIcon}
                            {expandButton}
                        </div>
                    }
                    title={name}
                    subheader={"(Array)"}
                />
                {items}
                {button !== "" ? <CardActions disableSpacing>{button}</CardActions> : ""}
            </Card>
        );
    }
}

class StringProperty extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            descriptionOpen: false
        };
    }
    handleChange(event) {
        this.props.onChange(this.props.name, event.target.value);
    }
    handleRemove() {
        this.props.onRemove(this.props.name);
    }
    handleDescriptionOpen() {
        this.setState({
            descriptionOpen: true
        });
    }
    handleDescriptionClose() {
        this.setState({
            descriptionOpen: false
        });
    }
    render() {
        var value = this.props.module;
        var readOnly = false;
        var onChange = this.handleChange.bind(this);
        var deleteButton = "";
        var hasError = this.props.errors !== undefined && this.props.errors.length > 0;
        var errorIcon = "";
        var inputCode;
        var inputStyle;
        var helpIcon = (
            <Grid item>
                <IconButton onClick={this.handleDescriptionOpen.bind(this)}>
                    <Icon>help</Icon>
                </IconButton>
            </Grid>
        );
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
        if (hasError) {
            var errorMessage = this.props.errors.pop().message;
            errorIcon = (
                <Grid item>
                    <IconButton>
                        <Tooltip title={errorMessage} arrow>
                            <Icon color={"error"}>error</Icon>
                        </Tooltip>
                    </IconButton>
                </Grid>
            );
        }
        if (!this.props.required) {
            deleteButton = (
                <Grid item>
                    <IconButton onClick={this.handleRemove.bind(this)}>
                        <Icon>delete</Icon>
                    </IconButton>
                </Grid>
            );
        }
        this.props.tabIndex.counter += 1;
        if (this.props.jsonSchema.hasOwnProperty("enum")) {
            inputStyle = (
                <Grid item>
                    <Select
                        className={"select"}
                        value={value}
                        onChange={onChange}
                        readOnly={readOnly}
                        inputProps={{ tabIndex: this.props.tabIndex.counter }}
                    >
                        {this.props.jsonSchema.enum.map(enumValue => {
                            return (
                                <MenuItem key={enumValue} value={enumValue}>
                                    {enumValue.toString()}
                                </MenuItem>
                            );
                        })}
                    </Select>
                </Grid>
            );
        } else {
            inputStyle = (
                <Grid item>
                    <Input
                        className={"select"}
                        type={"text"}
                        name={this.props.name}
                        value={value}
                        readOnly={readOnly}
                        onChange={onChange}
                        inputProps={{ tabIndex: this.props.tabIndex.counter }}
                    />
                </Grid>
            );
        }
        inputCode = (
            <FormControl error={hasError}>
                <FormLabel>{this.props.jsonSchema.title}</FormLabel>
                <Grid component="label" container alignItems="center" spacing={0}>
                    {inputStyle}
                    {errorIcon}
                    {helpIcon}
                    {deleteButton}
                </Grid>
            </FormControl>
        );
        return (
            <div>
                {inputCode}
                <Description
                    open={this.state.descriptionOpen}
                    content={this.props.jsonSchema.description}
                    onClose={this.handleDescriptionClose.bind(this)}
                />
            </div>
        );
    }
}

class IntegerProperty extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            descriptionOpen: false
        };
    }
    handleChange(event) {
        var value = Number(event.target.value);
        if (
            this.props.jsonSchema.hasOwnProperty("minimum") &&
            value < this.props.jsonSchema.minimum
        ) {
            this.props.onChange(this.props.name, this.props.jsonSchema.minimum);
        } else if (
            this.props.jsonSchema.hasOwnProperty("maximum") &&
            value > this.props.jsonSchema.maximum
        ) {
            this.props.onChange(this.props.name, this.props.jsonSchema.maximum);
        } else {
            this.props.onChange(this.props.name, value);
        }
    }
    handleRemove() {
        this.props.onRemove(this.props.name);
    }
    handleDescriptionOpen() {
        this.setState({
            descriptionOpen: true
        });
    }
    handleDescriptionClose() {
        this.setState({
            descriptionOpen: false
        });
    }
    render() {
        var value = this.props.module;
        var readOnly = false;
        var onChange = this.handleChange.bind(this);
        var min = null;
        var max = null;
        var deleteButton = "";
        var hasError = this.props.errors !== undefined && this.props.errors.length > 0;
        var errorIcon = "";
        var inputCode;
        var inputStyle;
        var helpIcon = (
            <Grid item>
                <IconButton onClick={this.handleDescriptionOpen.bind(this)}>
                    <Icon>help</Icon>
                </IconButton>
            </Grid>
        );
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
        if (hasError) {
            var errorMessage = this.props.errors.pop().message;
            errorIcon = (
                <Grid item>
                    <IconButton>
                        <Tooltip title={errorMessage} arrow>
                            <Icon color={"error"}>error</Icon>
                        </Tooltip>
                    </IconButton>
                </Grid>
            );
        }
        if (!this.props.required) {
            deleteButton = (
                <Grid item>
                    <IconButton onClick={this.handleRemove.bind(this)}>
                        <Icon>delete</Icon>
                    </IconButton>
                </Grid>
            );
        }
        this.props.tabIndex.counter += 1;
        inputStyle = (
            <Grid item>
                <Input
                    className={"select"}
                    type={"number"}
                    name={this.props.name}
                    value={value}
                    readOnly={readOnly}
                    inputProps={{
                        min: min,
                        max: max,
                        step: 1,
                        tabIndex: this.props.tabIndex.counter
                    }}
                    onChange={onChange}
                />
            </Grid>
        );
        inputCode = (
            <FormControl error={hasError}>
                <FormLabel>{this.props.jsonSchema.title}</FormLabel>
                <Grid component="label" container alignItems="center" spacing={0}>
                    {inputStyle}
                    {errorIcon}
                    {helpIcon}
                    {deleteButton}
                </Grid>
            </FormControl>
        );
        return (
            <div>
                {inputCode}
                <Description
                    open={this.state.descriptionOpen}
                    content={this.props.jsonSchema.description}
                    onClose={this.handleDescriptionClose.bind(this)}
                />
            </div>
        );
    }
}

class BooleanProperty extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            descriptionOpen: false
        };
    }
    handleChange() {
        this.props.onChange(this.props.name, !this.props.module);
    }
    handleRemove() {
        this.props.onRemove(this.props.name);
    }
    handleDescriptionOpen() {
        this.setState({
            descriptionOpen: true
        });
    }
    handleDescriptionClose() {
        this.setState({
            descriptionOpen: false
        });
    }
    render() {
        var value = this.props.module;
        var readOnly = false;
        var onChange = this.handleChange.bind(this);
        var deleteButton = "";
        var inputCode;
        var inputStyle;
        var helpIcon = (
            <Grid item>
                <IconButton onClick={this.handleDescriptionOpen.bind(this)}>
                    <Icon>help</Icon>
                </IconButton>
            </Grid>
        );
        if (this.props.jsonSchema.hasOwnProperty("default") && value === undefined) {
            value = this.props.jsonSchema.default;
        }
        if (this.props.jsonSchema.hasOwnProperty("const")) {
            value = this.props.jsonSchema.const;
            readOnly = true;
            onChange = null;
        }
        if (value === null) {
            value = false;
        }
        if (!this.props.required) {
            deleteButton = (
                <Grid item>
                    <IconButton onClick={this.handleRemove.bind(this)}>
                        <Icon>delete</Icon>
                    </IconButton>
                </Grid>
            );
        }
        this.props.tabIndex.counter += 1;
        inputStyle = (
            <React.Fragment>
                <Grid item>False</Grid>
                <Grid item>
                    <Switch
                        disabled={readOnly}
                        checked={value}
                        onChange={onChange}
                        inputProps={{ tabIndex: this.props.tabIndex.counter }}
                    />
                </Grid>
                <Grid item>True</Grid>
            </React.Fragment>
        );
        inputCode = (
            <FormControl>
                <FormLabel>{this.props.jsonSchema.title}</FormLabel>
                <Grid component="label" container alignItems="center" spacing={0}>
                    {inputStyle}
                    {helpIcon}
                    {deleteButton}
                </Grid>
            </FormControl>
        );
        return (
            <div>
                {inputCode}
                <Description
                    open={this.state.descriptionOpen}
                    content={this.props.jsonSchema.description}
                    onClose={this.handleDescriptionClose.bind(this)}
                />
            </div>
        );
    }
}

class NumberProperty extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            descriptionOpen: false
        };
    }
    handleChange(event) {
        var value = Number(event.target.value);
        if (
            this.props.jsonSchema.hasOwnProperty("minimum") &&
            value < this.props.jsonSchema.minimum
        ) {
            this.props.onChange(this.props.name, this.props.jsonSchema.minimum);
        } else if (
            this.props.jsonSchema.hasOwnProperty("maximum") &&
            value > this.props.jsonSchema.maximum
        ) {
            this.props.onChange(this.props.name, this.props.jsonSchema.maximum);
        } else {
            this.props.onChange(this.props.name, value);
        }
    }
    handleRemove() {
        this.props.onRemove(this.props.name);
    }
    handleDescriptionOpen() {
        this.setState({
            descriptionOpen: true
        });
    }
    handleDescriptionClose() {
        this.setState({
            descriptionOpen: false
        });
    }
    render() {
        var value = this.props.module;
        var readOnly = false;
        var onChange = this.handleChange.bind(this);
        var min = null;
        var max = null;
        var deleteButton = "";
        var hasError = this.props.errors !== undefined && this.props.errors.length > 0;
        var errorIcon = "";
        var inputCode;
        var inputStyle;
        var helpIcon = (
            <Grid item>
                <IconButton onClick={this.handleDescriptionOpen.bind(this)}>
                    <Icon>help</Icon>
                </IconButton>
            </Grid>
        );
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
        if (hasError) {
            var errorMessage = this.props.errors.pop().message;
            errorIcon = (
                <Grid item>
                    <IconButton>
                        <Tooltip title={errorMessage} arrow>
                            <Icon color={"error"}>error</Icon>
                        </Tooltip>
                    </IconButton>
                </Grid>
            );
        }
        if (!this.props.required) {
            deleteButton = (
                <Grid item>
                    <IconButton onClick={this.handleRemove.bind(this)}>
                        <Icon>delete</Icon>
                    </IconButton>
                </Grid>
            );
        }
        this.props.tabIndex.counter += 1;
        inputStyle = (
            <Grid item>
                <Input
                    className={"select"}
                    type={"number"}
                    name={this.props.name}
                    value={value}
                    readOnly={readOnly}
                    inputProps={{
                        min: min,
                        max: max,
                        step: 0.01,
                        tabIndex: this.props.tabIndex.counter
                    }}
                    onChange={onChange}
                />
            </Grid>
        );
        inputCode = (
            <FormControl error={hasError}>
                <FormLabel>{this.props.jsonSchema.title}</FormLabel>
                <Grid component="label" container alignItems="center" spacing={0}>
                    {inputStyle}
                    {errorIcon}
                    {helpIcon}
                    {deleteButton}
                </Grid>
            </FormControl>
        );
        return (
            <div>
                {inputCode}
                <Description
                    open={this.state.descriptionOpen}
                    content={this.props.jsonSchema.description}
                    onClose={this.handleDescriptionClose.bind(this)}
                />
            </div>
        );
    }
}

class MultipleTypesProperty extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            descriptionOpen: false
        };
    }
    handleChange(event) {
        var value = Number(event.target.value);
        if (isNaN(value) || event.target.value === "") {
            value = event.target.value;
        }
        this.props.onChange(this.props.name, value);
    }
    handleRemove() {
        this.props.onRemove(this.props.name);
    }
    handleDescriptionOpen() {
        this.setState({
            descriptionOpen: true
        });
    }
    handleDescriptionClose() {
        this.setState({
            descriptionOpen: false
        });
    }
    render() {
        var value = this.props.module;
        var readOnly = false;
        var onChange = this.handleChange.bind(this);
        var deleteButton = "";
        var hasError = this.props.errors !== undefined && this.props.errors.length > 0;
        var errorIcon = "";
        var inputCode;
        var inputStyle;
        var helpIcon = (
            <Grid item>
                <IconButton onClick={this.handleDescriptionOpen.bind(this)}>
                    <Icon>help</Icon>
                </IconButton>
            </Grid>
        );
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
        if (hasError) {
            var errorMessage = this.props.errors.pop().message;
            errorIcon = (
                <Grid item>
                    <IconButton>
                        <Tooltip title={errorMessage} arrow>
                            <Icon color={"error"}>error</Icon>
                        </Tooltip>
                    </IconButton>
                </Grid>
            );
        }
        if (!this.props.required) {
            deleteButton = (
                <Grid item>
                    <IconButton onClick={this.handleRemove.bind(this)}>
                        <Icon>delete</Icon>
                    </IconButton>
                </Grid>
            );
        }
        this.props.tabIndex.counter += 1;
        inputStyle = (
            <Grid item>
                <Input
                    className={"select"}
                    type={"text"}
                    name={this.props.name}
                    value={value}
                    readOnly={readOnly}
                    onChange={onChange}
                    inputProps={{ tabIndex: this.props.tabIndex.counter }}
                />
            </Grid>
        );
        inputCode = (
            <FormControl error={hasError}>
                <FormLabel>{this.props.jsonSchema.title}</FormLabel>
                <Grid component="label" container alignItems="center" spacing={0}>
                    {inputStyle}
                    {errorIcon}
                    {helpIcon}
                    {deleteButton}
                </Grid>
            </FormControl>
        );
        return (
            <div>
                {inputCode}
                <Description
                    open={this.state.descriptionOpen}
                    content={this.props.jsonSchema.description}
                    onClose={this.handleDescriptionClose.bind(this)}
                />
            </div>
        );
    }
}

class Description extends React.Component {
    handleClose() {
        this.props.onClose();
    }
    render() {
        var descContent;
        var contentParts = this.props.content.split("\n");
        descContent = (
            <DialogContent dividers>
                {contentParts.map((part, index) => {
                    return <p key={index}>{part}</p>;
                })}
            </DialogContent>
        );
        return (
            <Dialog
                disableBackdropClick
                open={this.props.open}
                fullWidth={false}
                maxWidth={"sm"}
                onEscapeKeyDown={this.handleClose.bind(this)}
                onBackdropClick={this.handleClose.bind(this)}
            >
                <DialogTitle>{"Description"}</DialogTitle>
                <Divider />
                {descContent}
                <DialogActions>
                    <Button
                        variant="outlined"
                        color="primary"
                        onClick={this.handleClose.bind(this)}
                    >
                        {"Close"}
                    </Button>
                </DialogActions>
            </Dialog>
        );
    }
}

{
    /* <Autocomplete
    freeSolo
    options={options}
    value={value}
    onChange={onChange}
    inputProps={{ tabIndex: this.props.tabIndex }}
    renderInput={params => <TextField {...params} readOnly={readOnly} />}
/>; */
}
