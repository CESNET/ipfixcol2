class Overlay extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            detailVisible: false
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