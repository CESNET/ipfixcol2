# Setup

You do not need to install any dependencies. To run the app open `index.html` file in your browser.

> **Note**: Internet Explorer is not supported.

# Add a new plugin

The process of adding a new plugin consists of 2 parts:
1.  [Creating a new JSON schema](#1-creating-a-new-json-schema).
2.  [Adding the JSON schema to the app](#2-adding-the-json-schema-to-the-app).

## 1. Creating a new JSON schema

### Basic structure
```
{
    "$schema": "http://json-schema.org/draft-07/schema#",
    "title": "Plugin name",
    "description": "Plugin description.|URL_to_documentation",
    "type": "object",
    "properties": {
        "propertyOne": {...},
        "propertyTwo": {...},
        "propertyThree": {...}
    },
    "required": ["propertyOne", "propertyTwo"]
}
```

| Parameter | Description |
| ------ | ------ |
| `title` | The name of the plugin. |
| `description`: | Description and URL to the documentation of the plugin in format `[description]\|[URL]`. The reason for this syntax is the lack of keyword to hold an URL. | 
| `type` | Type of value. There always be the value `"object"` as this is the root of the schema. |
| `properties` | An object of keys. Each key represents an actual property of the plugin configuration and has to have the exact name as the particular plugin property. |
| `required` | An array of keys in `properties` which has to be present in the configuration. |

All parameters are required.

### Property structure
```
<exampleProperty>text</exampleProperty>
```
```
"exampleProperty": {
    "title": "Property title",
    "description": "Property description.",
    "type": "string",
    ...
}
```
| Parameter | Description |
| --- | --- |
| `title` | The name of the property. |
| `description` | Description of the property. (does not support special syntax for URL) |
| `type` | Property data type. One of `string`, `boolean`, `integer`, `number`, `object` or `array`. |
> **Note**: Type `integer` allows only integer values. Type `number` will enable numbers with two decimal places.
> All parameters are required.

#### Optional property parameters
| Parameter | Description |
| ------ | ------ |
| `default` | Default value of the property. (e.g. `"text"`, `300`, `true`) |
| `const` | Constant value. (e.g. `"text"`, `300`, `true`) | 
| `minimum` | Minimum value of `integer` or `number` types. (e.g. `0`) |
| `maximum` | Maximum value of `integer` or `number` types. (e.g. `600`) |
| `minLength` | Minimum lenght of the `string` value. (e.g. `1`) |
| `maxLenght` | Maximum lenght of the `string` value. (e.g. `20`) |
| `enum` | Array of valid values. (e.g. `[true, false]`, `["option1", "option2", "option3"]`) |

For a complete list of available parameters, see [the documentation of the JSON schema](https://json-schema.org/draft/2019-09/json-schema-validation.html).

### Recurring property structure (Array property)
```
<recurringProperty>occurrence1</recurringProperty>
<recurringProperty>occurrence2</recurringProperty>
<recurringProperty>occurrence3</recurringProperty>
```
```
"recurringProperty": {
    "title": "Recuring properties",
    "type": "array",
    "minItems": 1,
    "maxItems": 8,
    "uniqueItems": true,
    "items": {
        "title": "Recuring property",
        "description": "Property description.",
        "type": "string"
    }
}
```
| Parameter | Description |
| ------ | ------ |
| `minItems` | Minimum number of items of the array. (optional) |
| `maxItems` | Maximum number of items of the array. (optional) | 
| `uniqueItems` | Enforces uniqueness of array items. (optional) |
| `items` | Definition of array items. (required) |
> **Note**: Parameter `description` is only present inside parameter `items`.

### Nested parameters structure (Object property)
```
<params>
    <param1></param1>
    <param2></param2>
    ...
</params>
```
```
"params": {
    "title": "Parameters",
    "description": "...",
    "type": "object",
    "properties": {
        "param1": {...},
        "param2": {...},
        ...
    },
    required: ["param1", "param2"]
}
```
The structure is the same as the basic structure of the file.

### Examples
Feel free to use already created schemas for your inspiration.
*  [Input plugins](./schemas/input)
*  [Intermediate plugins](./schemas/intermediate)
*  [Output plugins](./schemas/output)

## 2. Adding the JSON schema to the app
First, depending on the type of the plugin, place the schema into the correct folder (`/schemas/input`, `/schemas/intermediate`, `/schemas/output`).
Then add the filename to the `/config/config.json` file. See the example below.

### Example
Let's say we want to add the [Dummy](https://github.com/CESNET/ipfixcol2/tree/master/src/plugins/output/dummy) plugin.
It is an output type plugin, so we place it into `/schemas/output` folder (`/schemas/output/dummy.json`).

And than we add the filename to the `/config/config.json` file.
```
{
    "schemaLocations": {
        "input": {
            ...
        },
        "intermediate": {
            ...
        },
        "output": {
            "path": "./schemas/output/",
            "pluginSchemas": [..., "dummy.json"]
        },
        ...
    }
}
```

# Production version
To create a production version of the app, change React and MaterialUI scripts inside `index.html` file from `development.js` to `production.min.js`.