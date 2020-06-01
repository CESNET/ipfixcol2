// import from MaterialUI
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
    createMuiTheme,
    ThemeProvider,
} = MaterialUI;

// x2js convertor
const x2js = new X2JS();

// Ajv - JSON schema validator
const ajv = new Ajv({ allErrors: true });

const colors = ["blue", "orange", "red"];
const columnDataPaths = [
    ".ipfixcol2.inputPlugins.input",
    ".ipfixcol2.intermediatePlugins.intermediate",
    ".ipfixcol2.outputPlugins.output",
];
const columnNames = ["Input plugins", "Intermediate plugins", "Output plugins"];
const cookieExpirationDays = 365;
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

const defaultSettings = {
    indentType: indentationTypes[0],
    indentNumber: 2,
    showConfirmationDialogs: true,
};

const mainTheme = createMuiTheme({
    palette: {
        primary: {
            main: "#1e88e5",
        },
    },
});

const grayTheme = createMuiTheme({
    palette: {
        primary: {
            main: "#424242",
        },
    },
});
