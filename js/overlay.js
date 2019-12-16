function moduleCreate(jsonSchema) {
    var newModule = {};
    for (var i in jsonSchema.required) {
        moduleSetProperty(newModule, jsonSchema.required[i], jsonSchema.properties[jsonSchema.required[i]]);
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
                moduleSetProperty(module[propertyName], jsonSchema.required[i], jsonSchema.properties[jsonSchema.required[i]]);
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
                    : moduleCreate(this.props.jsonSchema)
        };
    }
    render() {
        return (
            <div className={"overlay"}>
                <div className={"content"}>
                    <Button variant="outlined" color="primary" onClick={this.props.onCancel}>
                        Cancel
                    </Button>
                </div>
            </div>
        );
    }
}
