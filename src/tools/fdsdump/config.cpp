#include "config.hpp"
#include <cstdio>
#include <vector>
#include <regex>
#include "common.hpp"
#include "information_elements.hpp"

class ArgParser
{
public:
    ArgParser(int argc, char **argv);

    bool
    next();

    std::string
    arg();

private:
    int m_argc;
    char **m_argv;
    int m_arg_index = 0;
};

ArgParser::ArgParser(int argc, char **argv)
    : m_argc(argc), m_argv(argv)
{
}

bool
ArgParser::next()
{
    if (m_arg_index == m_argc - 1) {
        return false;
    }
    m_arg_index++;
    return true;
}

std::string
ArgParser::arg()
{
    return m_argv[m_arg_index];
}

static void
usage()
{
    const char *text =
    "Usage: fdsdump [options]\n"
    "  -h         Show this help\n"
    "  -r path    FDS input file\n"
    "  -f expr    Input filter\n"
    "  -of expr   Output filter\n"
    "  -c num     Max number of records to read\n"
    "  -a keys    Aggregator keys (e.g. srcip,dstip,srcport,dstport)\n"
    "  -av values Aggregator values // TODO\n"
    "  -s field   Field to sort on (e.g. bytes, packets, flows)\n"
    "  -n num     Maximum number of records to write\n"
    ;
    fprintf(stderr, text);
}

static void
missing_arg(const char *opt)
{
    fprintf(stderr, "Missing argument for %s", opt);
}

static int
parse_aggregate_key_config(const std::string &options, ViewDefinition &view_def, fds_iemgr_t &iemgr)
{
    for (const auto &key : string_split(options, ",")) {
        ViewField field = {};

        std::regex subnet_regex{"([a-zA-Z0-9:]+)/(\\d+)"};
        std::smatch m;

        if (std::regex_match(key, m, subnet_regex)) {
            field.name = key;
            auto prefix_length = std::stoul(m[2].str());

            if (m[1] == "srcipv4") {
                field.pen = IPFIX::iana;
                field.id = IPFIX::sourceIPv4Address;
                field.data_type = DataType::IPv4Address;
                field.size = sizeof(ViewValue::ipv4);
                field.kind = ViewFieldKind::IPv4SubnetKey;

            } else if (m[1] == "dstipv4") {
                field.pen = IPFIX::iana;
                field.id = IPFIX::destinationIPv4Address;
                field.data_type = DataType::IPv4Address;
                field.size = sizeof(ViewValue::ipv4);
                field.kind = ViewFieldKind::IPv4SubnetKey;

            } else if (m[1] == "srcipv6") {
                field.pen = IPFIX::iana;
                field.id = IPFIX::sourceIPv6Address;
                field.data_type = DataType::IPv6Address;
                field.size = sizeof(ViewValue::ipv6);
                field.kind = ViewFieldKind::IPv6SubnetKey;

            } else if (m[1] == "dstipv6") {
                field.pen = IPFIX::iana;
                field.id = IPFIX::destinationIPv6Address;
                field.data_type = DataType::IPv6Address;
                field.size = sizeof(ViewValue::ipv6);
                field.kind = ViewFieldKind::IPv6SubnetKey;

            } else if (m[1] == "ipv4") {
                field.data_type = DataType::IPv4Address;
                field.size = sizeof(ViewValue::ipv4);
                field.kind = ViewFieldKind::BidirectionalIPv4SubnetKey;
                view_def.bidirectional = true;

            } else if (m[1] == "ipv6") {
                field.data_type = DataType::IPv6Address;
                field.size = sizeof(ViewValue::ipv6);
                field.kind = ViewFieldKind::BidirectionalIPv6SubnetKey;
                view_def.bidirectional = true;

            } else {
                const fds_iemgr_elem *elem = fds_iemgr_elem_find_name(&iemgr, m[1].str().c_str());

                if (!elem) {
                    fprintf(stderr, "Invalid aggregation key \"%s\" - element not found\n", key.c_str());
                    return 1;
                }

                if (elem->data_type != FDS_ET_IPV4_ADDRESS && elem->data_type != FDS_ET_IPV6_ADDRESS) {
                    fprintf(stderr, "Invalid aggregation key \"%s\" - not an IP address\n", key.c_str());
                    return 1;
                }

                field.pen = elem->scope->pen;
                field.id = elem->id;
                if (elem->data_type == FDS_ET_IPV4_ADDRESS) {
                    field.data_type = DataType::IPv4Address;
                    field.size = sizeof(ViewValue::ipv4);
                    field.kind = ViewFieldKind::IPv4SubnetKey;
                } else if (elem->data_type == FDS_ET_IPV6_ADDRESS) {
                    field.data_type = DataType::IPv6Address;
                    field.size = sizeof(ViewValue::ipv6);
                    field.kind = ViewFieldKind::IPv6SubnetKey;
                }
            }

            if (field.data_type == DataType::IPv4Address && (prefix_length <= 0 || prefix_length > 32)) {
                fprintf(stderr, "Invalid aggregation key \"%s\" - invalid prefix length %lu for IPv4 address\n", key.c_str(), prefix_length);
                return 1;
            } else if (field.data_type == DataType::IPv6Address && (prefix_length <= 0 || prefix_length > 128)) {
                fprintf(stderr, "Invalid aggregation key \"%s\" - invalid prefix length %lu for IPv6 address\n", key.c_str(), prefix_length);
                return 1;
            }

            field.extra.prefix_length = prefix_length;
            view_def.keys_size += field.size;

        } else if (key == "srcip") {
            field.data_type = DataType::IPAddress;
            field.kind = ViewFieldKind::SourceIPAddressKey;
            field.name = "srcip";
            field.size = sizeof(ViewValue::ip);
            view_def.keys_size += sizeof(ViewValue::ip);

        } else if (key == "dstip") {
            field.data_type = DataType::IPAddress;
            field.kind = ViewFieldKind::DestinationIPAddressKey;
            field.name = "dstip";
            field.size = sizeof(ViewValue::ip);
            view_def.keys_size += sizeof(ViewValue::ip);

        } else if (key == "srcport") {
            field.data_type = DataType::Unsigned16;
            field.kind = ViewFieldKind::VerbatimKey;
            field.pen = IPFIX::iana;
            field.id = IPFIX::sourceTransportPort;
            field.name = "srcport";
            field.size = sizeof(ViewValue::u16);
            view_def.keys_size += sizeof(ViewValue::u16);

        } else if (key == "dstport") {
            field.data_type = DataType::Unsigned16;
            field.kind = ViewFieldKind::VerbatimKey;
            field.pen = IPFIX::iana;
            field.id = IPFIX::destinationTransportPort;
            field.name = "dstport";
            field.size = sizeof(ViewValue::u16);
            view_def.keys_size += sizeof(ViewValue::u16);

        } else if (key == "proto") {
            field.data_type = DataType::Unsigned8;
            field.kind = ViewFieldKind::VerbatimKey;
            field.pen = IPFIX::iana;
            field.id = IPFIX::protocolIdentifier;
            field.name = "proto";
            field.size = sizeof(ViewValue::u8);
            view_def.keys_size += sizeof(ViewValue::u8);

        } else if (key == "ip") {
            field.data_type = DataType::IPAddress;
            field.kind = ViewFieldKind::BidirectionalIPAddressKey;
            field.name = "ip";
            field.size = sizeof(ViewValue::ip);
            view_def.keys_size += sizeof(ViewValue::ip);
            view_def.bidirectional = true;

        } else if (key == "port") {
            field.data_type = DataType::Unsigned16;
            field.kind = ViewFieldKind::BidirectionalPortKey;
            field.name = "port";
            field.size = sizeof(ViewValue::u16);
            view_def.keys_size += sizeof(ViewValue::u16);
            view_def.bidirectional = true;

        } else {
            const fds_iemgr_elem *elem = fds_iemgr_elem_find_name(&iemgr, key.c_str());
            if (!elem) {
                fprintf(stderr, "Invalid aggregation key \"%s\" - element not found\n", key.c_str());
                return 1;
            }

            switch (elem->data_type) {
            case FDS_ET_UNSIGNED_8:
                field.data_type = DataType::Unsigned8;
                field.size = sizeof(ViewValue::u8);
                break;
            case FDS_ET_UNSIGNED_16:
                field.data_type = DataType::Unsigned16;
                field.size = sizeof(ViewValue::u16);
                break;
            case FDS_ET_UNSIGNED_32:
                field.data_type = DataType::Unsigned32;
                field.size = sizeof(ViewValue::u32);
                break;
            case FDS_ET_UNSIGNED_64:
                field.data_type = DataType::Unsigned64;
                field.size = sizeof(ViewValue::u64);
                break;
            case FDS_ET_SIGNED_8:
                field.data_type = DataType::Signed8;
                field.size = sizeof(ViewValue::i8);
                break;
            case FDS_ET_SIGNED_16:
                field.data_type = DataType::Signed16;
                field.size = sizeof(ViewValue::i16);
                break;
            case FDS_ET_SIGNED_32:
                field.data_type = DataType::Signed32;
                field.size = sizeof(ViewValue::i32);
                break;
            case FDS_ET_SIGNED_64:
                field.data_type = DataType::Signed64;
                field.size = sizeof(ViewValue::i64);
                break;
            case FDS_ET_IPV4_ADDRESS:
                field.data_type = DataType::IPv4Address;
                field.size = sizeof(ViewValue::ipv4);
                break;
            case FDS_ET_IPV6_ADDRESS:
                field.data_type = DataType::IPv6Address;
                field.size = sizeof(ViewValue::ipv6);
                break;
            default:
                fprintf(stderr, "Invalid aggregation key \"%s\" - data type not supported\n", key.c_str());
                return 1;
            }

            field.kind = ViewFieldKind::VerbatimKey;
            field.pen = elem->scope->pen;
            field.id = elem->id;
            field.name = elem->name;
            view_def.keys_size += field.size;
        }

        view_def.key_fields.push_back(field);
    }

    return 0;
}

static int
parse_aggregate_value_config(const std::string &options, ViewDefinition &view_def, fds_iemgr_t &iemgr)
{
    auto values = string_split(options, ",");

    for (const auto &value : values) {
        ViewField field = {};

        if (value == "packets") {
            field.data_type = DataType::Unsigned64;
            field.pen = IPFIX::iana;
            field.id = IPFIX::packetDeltaCount;
            field.kind = ViewFieldKind::SumAggregate;
            field.name = "packets";
            field.size = sizeof(ViewValue::u64);
            view_def.values_size += sizeof(ViewValue::u64);

        } else if (value == "bytes") {
            field.data_type = DataType::Unsigned64;
            field.pen = IPFIX::iana;
            field.id = IPFIX::octetDeltaCount;
            field.kind = ViewFieldKind::SumAggregate;
            field.name = "bytes";
            field.size = sizeof(ViewValue::u64);
            view_def.values_size += sizeof(ViewValue::u64);

        } else if (value == "flows") {
            field.data_type = DataType::Unsigned64;
            field.kind = ViewFieldKind::FlowCount;
            field.name = "flows";
            field.size = sizeof(ViewValue::u64);
            view_def.values_size += sizeof(ViewValue::u64);

        } else if (value == "inpackets") {
            field.data_type = DataType::Unsigned64;
            field.pen = IPFIX::iana;
            field.id = IPFIX::packetDeltaCount;
            field.kind = ViewFieldKind::SumAggregate;
            field.name = "inpackets";
            field.size = sizeof(ViewValue::u64);
            field.direction = Direction::In;
            view_def.values_size += sizeof(ViewValue::u64);

        } else if (value == "inbytes") {
            field.data_type = DataType::Unsigned64;
            field.pen = IPFIX::iana;
            field.id = IPFIX::octetDeltaCount;
            field.kind = ViewFieldKind::SumAggregate;
            field.name = "inbytes";
            field.size = sizeof(ViewValue::u64);
            field.direction = Direction::In;
            view_def.values_size += sizeof(ViewValue::u64);

        } else if (value == "inflows") {
            field.data_type = DataType::Unsigned64;
            field.kind = ViewFieldKind::FlowCount;
            field.name = "inflows";
            field.size = sizeof(ViewValue::u64);
            field.direction = Direction::In;
            view_def.values_size += sizeof(ViewValue::u64);

        } else if (value == "outpackets") {
            field.data_type = DataType::Unsigned64;
            field.pen = IPFIX::iana;
            field.id = IPFIX::packetDeltaCount;
            field.kind = ViewFieldKind::SumAggregate;
            field.name = "outpackets";
            field.size = sizeof(ViewValue::u64);
            field.direction = Direction::Out;
            view_def.values_size += sizeof(ViewValue::u64);

        } else if (value == "outbytes") {
            field.data_type = DataType::Unsigned64;
            field.pen = IPFIX::iana;
            field.id = IPFIX::octetDeltaCount;
            field.kind = ViewFieldKind::SumAggregate;
            field.name = "outbytes";
            field.size = sizeof(ViewValue::u64);
            field.direction = Direction::Out;
            view_def.values_size += sizeof(ViewValue::u64);

        } else if (value == "outflows") {
            field.data_type = DataType::Unsigned64;
            field.kind = ViewFieldKind::FlowCount;
            field.name = "outflows";
            field.size = sizeof(ViewValue::u64);
            field.direction = Direction::Out;
            view_def.values_size += sizeof(ViewValue::u64);

        } else {
            fprintf(stderr, "Invalid aggregation value \"%s\"\n", value.c_str());
            return 1;
        }
        view_def.value_fields.push_back(field);
    }

    return 0;
}

int
config_from_args(int argc, char **argv, Config &config, fds_iemgr_t &iemgr)
{
    config = {};

    ArgParser parser{argc, argv};
    while (parser.next()) {
        if (parser.arg() == "-h") {
            usage();
            return 1;
        } else if (parser.arg() == "-r") {
            if (!parser.next()) {
                missing_arg("-r");
                return 1;
            }
            config.input_file = parser.arg();
        } else if (parser.arg() == "-f") {
            if (!parser.next()) {
                missing_arg("-f");
                return 1;
            }
            config.input_filter = parser.arg();
        } else if (parser.arg() == "-of") {
            if (!parser.next()) {
                missing_arg("-of");
                return 1;
            }
            config.output_filter = parser.arg();
        } else if (parser.arg() == "-c") {
            if (!parser.next()) {
                missing_arg("-c");
                return 1;
            }
            config.max_input_records = std::stoul(parser.arg());
        } else if (parser.arg() == "-a") {
            if (!parser.next()) {
                missing_arg("-a");
                return 1;
            }
            if (parse_aggregate_key_config(parser.arg(), config.view_def, iemgr) == 1) {
                return 1;
            }
        } else if (parser.arg() == "-av") {
            if (!parser.next()) {
                missing_arg("-av");
                return 1;
            }
            if (parse_aggregate_value_config(parser.arg(), config.view_def, iemgr) == 1) {
                return 1;
            }
        } else if (parser.arg() == "-n") {
            if (!parser.next()) {
                missing_arg("-n");
                return 1;
            }
            config.max_output_records = std::stoul(parser.arg());
        } else if (parser.arg() == "-s") {
            if (!parser.next()) {
                missing_arg("-s");
                return 1;
            }
            config.sort_field = parser.arg();
            if (!is_one_of(config.sort_field, {"bytes", "packets", "flows", "inbytes", "inpackets", "inflows", "outbytes", "outpackets", "outflows"})) {
                fprintf(stderr, "Invalid sort field \"%s\"\n", config.sort_field.c_str());
                return 1;
            }
        } else {
            fprintf(stderr, "Unknown argument %s\n", parser.arg().c_str());
            return 1;
        }
    }

    if (config.input_file.empty()) {
        usage();
        return 1;
    }

    if (config.input_filter.empty()) {
        config.input_filter = "true";
    }

    if (config.output_filter.empty()) {
        config.output_filter = "true";
    }

    return 0;
}