
// import from MaterialUI
const {
    Allert,
    AppBar,
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
    LinearProgress,
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
    Typography,
    createMuiTheme,
    ThemeProvider,
} = MaterialUI;

// x2js convertor
const X2JS = new X2JS();

// Ajv - JSON schema validator
const AJV = new Ajv({ allErrors: true });

const COLORS = ["blue", "orange", "red"];
const SECTION_DATA_PATH = [
    ".ipfixcol2.inputPlugins.input",
    ".ipfixcol2.intermediatePlugins.intermediate",
    ".ipfixcol2.outputPlugins.output",
];
const COLUMN_NAMES = ["Input plugins", "Intermediate plugins", "Output plugins"];
const COOKIE_EXPIRATION_DAYS = 365;
const DEFAULT_CONFIG = {
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
const INDENTATION_TYPES = [
    { name: "Space", character: " " },
    { name: "Tab", character: "\t" },
];
const INDENTATION_SPACES = { min: 0, max: 8 };
const DEFAULT_SETTINGS = {
    indentType: INDENTATION_TYPES[0],
    indentNumber: 2,
    showConfirmationDialogs: true,
};

const MAIN_THEME = createMuiTheme({
    palette: {
        primary: {
            main: "#1e88e5",
        },
    },
});
const GRAY_THEME = createMuiTheme({
    palette: {
        primary: {
            main: "#424242",
        },
    },
});
