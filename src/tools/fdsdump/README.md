# fdsdump [preview]

Reads flow data stored in fdsdump format and displays them in a specified way.

Supports sorting, filtering, aggregation, and selection of fields to display. Display can be in form of a formatted table output or a json output.

## Command line options
- `-r` — FDS files to read, can also be a glob pattern and can be specified multiple times to select more files

- `-A` — Aggregator keys (TBD)

- `-S` — Aggregated values (TBD)

- `-I` — Run in statistics mode

- `-F`, `--filter` — Filter the flow records based on a filter expression

- `-o`, `--output` — The output format

- `-O`, `--order` — The fields to order by

- `-c`, `--limit` — Limit the number of records processed

- `--no-biflow-autoignore` — Disable smart ignore functionality of empty biflow records


## Modes
### Statistics mode

Selected using the `-I` flag

Prints basic information about the flows contained in the FDS files, such as packet counts, byte counts, flow counts in either directions in case of biflow.


### Lister mode

Displays flow records in a specified format (table or json). Flow records can be sorted and ordered based on field values.

### Aggregation mode

TBD


## Output formats

- `json-raw` — prints flow records in the json format with all their fields
- `json` — json output of selected fields
- `table` — table output of selected fields

Fields can be specified for `json` and `table` outputs as such: `json:srcip,dstip,srcport,dstport,flows,packets,bytes`

## Filtering expressions

Fields to filter on can be specified by their full name, or by aliases which are defined for the most commonly used fields, e.g. ip, srcip, dstport, port, srcport, packets, bytes, flows, and more. The full list of aliases can be seen in the aliases.xml file supplied by libfds, where more can be defined.

### Supported operations

- Comparison operators `==`, `<`, `>`, `<=`, `>=`, `!=`. If the comparison operator is ommited, the default comparison is `==`.

- The `contains`, `startswith` and `endswith` operators for substring comparison, e.g. `DNSName contains "example"`.

- Arithmetic operations `+`, `-`, `*`, `/`, `%`.

- Bitwise operations not `~`, or `|`, and `&`, xor `^`.

- The `in` operator for list comparison, e.g. `port in [80, 443]`.

- The logical `and`, `or`, `not` operators.

### Value formats

- Numbers can be integer or floating point. Integer numbers can also be written in their hexadecimal or binary form using the `0x` or `0b` prefix. Floating point numbers also support the exponential notation such as `1.2345e+2`. A number can be explicitly unsigned using the `u` suffix. Numbers also support size suffixes `B`, `k`, `M`, `G`, `T`, and time suffixes `ns`, `us`, `ms`, `s`, `m`, `d`.

- Strings are values enclosed in a pair of double quotes `"`. Supported escape sequences are `\n`, `\r`, `\t` and `\"`. The escape sequences to write characters using their octal or hexadecimal value are also supported, e.g. `\ux22` or `\042`.

- IP addresses are written in their usual format, e.g. `127.0.0.1` or `1234:5678:9abc:def1:2345:6789:abcd:ef12`. The shortened IPv6 version is also supported, e.g. `::ff`. IP addresses can also contain a suffix specifying their prefix length, e.g. `10.0.0.0/16`.

- MAC addresses are written in their usual format, e.g. `12:34:56:78:9a:bc`.

- Timestamps use the ISO timestamp format, e.g. `2020-04-05T24:00Z`.

Filtering expressions can be joined by logical operators and even nested using parentheses.


### Example expressions

- `ip == 10.0.0.1` — Exact match on source or destination IP address, specified using an alias
- `ip == 10.0.0.0/8` — Subnet match on source or detination IP address
- `ip 10.0.0.0/8` — Omitting comparison operator defaults to ==
-`ip 10.0.0.0/8 and port 80` — Also match source or destination port using an alias
- `ip 10.0.0.0/8 and (port 80 or port 443)` — Match multiple ports
- `ip 10.0.0.0/8 and port in [80, 443]` — A shorthand list syntax for specifying multiple ports
- `ip 10.0.0.0/8 and port in [80, 443] and packets > 1000` — Using an alias for `iana:packetDeltaCount`
- `ip 10.0.0.0/8 and port in [80, 443] and packets > 1M` — Using number units
- `cesnet:DNSName contains "google"` — String operations and specification of a field by its full name
`cesnet:DNSName endswith ".com"` — More string operations
- `iana:sourceTransportPort == 80 and iana:octetDeltaCount > 100` — Specification of fields by their full name

Sorting
---------

Fields to order by is specified by the `-O` flag such as: `-O packets/desc,flows/desc`. Field names can be aliases or their full name. Possible ordering options are `asc` for ascending or `desc` for descending.

Example usage
--------------

- Write out all the flow records in JSON format
    ```
    fdsdump -r ./flows.fds -o 'json-raw'
    ```

- Write out the source IP, source port, destination IP, destination port, number of packets and number of bytes of each flow in JSON format.
    ```
    fdsdump -r ./flows.fds -o 'json:srcip,srcport,dstip,dstport,packets,bytes'
    ```

- Write out the source IP, source port, destination IP, destination port, number of packets and number of bytes of the top 10 flows with most packets as a formatted table.
    ```
    fdsdump -r ./flows.fds -o 'table:srcip,srcport,dstip,dstport,packets,bytes' -O 'packets/desc' -c 10
    ```

- Write out the source IP, source port, destination IP, destination port, number of packets and number of bytes of the top 10 flows with most packets from where either the source or destination address is from the subnet 12.34.56.0/24 as a formatted table.
    ```
    fdsdump -r ./flows.fds -o 'table:srcip,srcport,dstip,dstport,packets,bytes' -O 'packets/desc' -c 10 -F 'ip 12.34.56.0/24'
    ```
