#!/usr/bin/env python3

"""
A helper script to help with ipfixcol2 clickhouse output plugin onboarding.

Author: Michal Sedlak <sedlakm@cesnet.cz>

Copyright(c) 2025 CESNET z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause
"""


from pathlib import Path
import argparse
import json
import subprocess
import sys
import tempfile
import time


RED = "\x1b[0;31m"
GREEN = "\x1b[0;32m"
YELLOW = "\x1b[0;33m"
RESET = "\x1b[0m"


IPFIX_TYPE_TO_CLICKHOUSE = {
    "unsigned8": "UInt8",
    "unsigned16": "UInt16",
    "unsigned32": "UInt32",
    "unsigned64": "UInt64",
    "signed8": "Int8",
    "signed16": "Int16",
    "signed32": "Int32",
    "signed64": "Int64",
    "ipv4Address": "IPv4",
    "ipv6Address": "IPv6",
    "string": "String",
    "dateTimeSeconds": "DateTime",
    "dateTimeMilliseconds": "DateTime64(3)",
    "dateTimeMicroseconds": "DateTime64(6)",
    "dateTimeNanoseconds": "DateTime64(9)",
    "macAddress": "UInt64",
    "float32": "Float32",
    "float64": "Float64",
    "octetArray": "String",
}


class Field:
    def __init__(self, name, data_type, note=None, alias_of=[], in_percent_of_records=None):
        self.name = name
        self.data_type = data_type
        self.note = note
        self.alias_of = alias_of
        self.in_percent_of_records = in_percent_of_records

    def is_(self, name):
        return self.name == name or name in self.alias_of

    @property
    def comment(self):
        s = ""
        if self.note:
            s += self.note

        if s:
            s += "; "
        s += f"in {self.in_percent_of_records:.2f}% of records"

        if self.alias_of:
            s += f"; alias of {'/'.join(self.alias_of)}"

        return s


def have_elem(data, name):
    return any(elem["name"] == name for elem in data["elements"])


def is_exclusive_alias(data, alias):
    return sum(alias in elem["aliases"] for elem in data["elements"]) == 1


def replace_fields(fields, names, field):
    indices = []
    for name in names:
        try:
            indices.append(next(i for i, field in enumerate(fields) if field.name == name or name in field.alias_of))
        except:
            pass
    index = min(indices)

    percent = 0.0
    for i in indices:
        percent += fields[i].in_percent_of_records

    fields = [field for i, field in enumerate(fields) if i not in indices]
    field.in_percent_of_records = percent
    fields.insert(index, field)

    return fields


def find_field(fields, any_of_names):
    for name in any_of_names:
        fields = [field for field in fields if field.is_(name)]
        if fields:
            return fields[0]
    return None


def generate_config(args, fields):
    config = f"""\
<ipfixcol2>
    <inputPlugins>
        <input>
            <name>{args.type} input plugin</name>
            <plugin>{args.type}</plugin>
            <params>
                <localPort>{args.port}</localPort>
                <localIPAddress>{args.address}</localIPAddress>
            </params>
        </input>
    </inputPlugins>
    <outputPlugins>
        <output>
            <name>ClickHouse output</name>
            <plugin>clickhouse</plugin>
            <params>
                <connection>
                    <endpoints>
                        <!-- One or more ClickHouse databases (endpoints) -->
                        <endpoint>
                            <host>CHANGEME.com</host>
                            <port>9000</port>
                        </endpoint>
                    </endpoints>
                    <user>CHANGEME</user>
                    <password>CHANGEME</password>
                    <database>CHANGEME</database>
                    <table>flows</table>
                </connection>
                <inserterThreads>16</inserterThreads>
                <blocks>128</blocks>
                <blockInsertThreshold>500000</blockInsertThreshold>
                <splitBiflow>true</splitBiflow>
                <nonblocking>true</nonblocking>
                <columns>
"""
    for field in fields:
        config += f"""\
                    <column>
                        <name>{field.name}</name>
                        <source>{field.name}</source>
"""
        if field.comment:
            config += f"""\
                        <!-- {field.comment} -->
"""
        config += """\
                    </column>
"""
    config += """\
                </columns>
            </params>
        </output>
    </outputPlugins>
</ipfixcol2>"""

    return config


def generate_schema(fields):
    flowstart_field = find_field(fields, ["iana:flowStartSeconds", "iana:flowStartMilliseconds",
                                          "iana:flowStartMicroseconds", "iana:flowStartNanoseconds"])
    srcip_field = find_field(fields, ["iana:sourceIPv4Address", "iana:sourceIPv6Address"])
    dstip_field = find_field(fields, ["iana:destinationIPv4Address", "iana:destinationIPv6Address"])

    schema = "CREATE TABLE flows(\n"
    for field, ending in zip(fields, (len(fields) - 1) * [","] + [""]):
        schema += f'    "{field.name}" {field.data_type}{ending}'
        if field.comment:
            schema += f" -- {field.comment}"
        schema += "\n"
    schema += "    -- Tip: You may also want to add indexes for better lookup performance\n"
    if srcip_field:
        schema += f'    -- INDEX "{srcip_field.name}_index" "{srcip_field.name}" TYPE bloom_filter GRANULARITY 16,\n'
    if dstip_field:
        schema += f'    -- INDEX "{dstip_field.name}_index" "{dstip_field.name}" TYPE bloom_filter GRANULARITY 16,\n'
    if not dstip_field and not srcip_field:
        schema += f'    -- e.g.: INDEX "srcip_index" "srcip" TYPE bloom_filter GRANULARITY 16,\n'
        schema += f'    --       INDEX "dstip_index" "dstip" TYPE bloom_filter GRANULARITY 16,\n'
    schema += ")\n"
    schema += "ENGINE MergeTree\n"
    schema += " -- Tip: You may also want to partition and order by flow timestamps\n"
    if flowstart_field:
        schema += f' -- PARTITION BY toStartOfInterval("{flowstart_field.name}", INTERVAL 1 HOUR)\n'
        schema += f' -- ORDER BY "{flowstart_field.name}"\n'
    else:
        schema += ' -- e.g.: PARTITION BY toStartOfInterval("flowstart", INTERVAL 1 HOUR)\n'
        schema += ' --       ORDER BY "flowstart"\n'
    return schema.strip()


def confirm_yesno(prompt):
    while True:
        yn = input(f"{YELLOW}{prompt} [y/N] {RESET}").lower().strip()
        if yn in ["y", "yes"]:
            return True
        elif yn in ["n", "no", ""]:
            return False


def main():
    class CustomFormatter(argparse.ArgumentDefaultsHelpFormatter, argparse.RawDescriptionHelpFormatter):
        pass
    parser = argparse.ArgumentParser(formatter_class=CustomFormatter)
    parser.description = """\
Script to help with ClickHouse output set-up.

The script runs IPFIXcol2 to collect a sample of data, which is then analyzed
and used for a generation of a ClickHouse schema and a corresponding ClickHouse
output plugin config file.
"""
    parser.epilog = """\
example:
    $ schema-helper.py -i 60 -p 4739 -t tcp
    Collect data for 60 seconds, listen on port 4739 using TCP.
"""
    parser.add_argument("-a", "--address", help="the local IP address, i.e. interface address; empty = all interfaces", type=str, default="")
    parser.add_argument("-p", "--port", help="the local port", type=int, default=4739)
    parser.add_argument("-i", "--interval", help="the collection interval in seconds", type=int, default=60)
    parser.add_argument("-t", "--type", help="the input protocol type", choices=["tcp", "udp"], type=str.lower, default="udp")
    parser.add_argument("-s", "--schema-file", help="the output schema file", type=Path, default=Path("schema.sql"))
    parser.add_argument("-c", "--config-file", help="the output config file", type=Path, default=Path("config.xml"))
    parser.add_argument("-o", "--overwrite", help="overwrite the output files without asking if they already exist", type=bool, default=False)
    args = parser.parse_args()

    if subprocess.run("command -v ipfixcol2", shell=True, check=False, capture_output=True).returncode != 0:
        print(RED)
        print("ERROR: Could not find \"ipfixcol2\" executable.")
        print()
        print("Is ipfixcol2 installed?")
        print(RESET)
        sys.exit(1)

    if not "clickhouse" in subprocess.run(["ipfixcol2", "-L"], capture_output=True).stdout.decode():
        print(RED)
        print("ERROR: ClickHouse plugin not found.")
        print()
        print("Ensure your ipfixcol2 is up-to-date and is compiled including \"extra plugins\" (see README).")
        print(RESET)
        sys.exit(1)

    tmpdir = Path(tempfile.mkdtemp())

    config = f"""
    <ipfixcol2>
        <inputPlugins>
            <input>
                <name>{args.type} input plugin</name>
                <plugin>{args.type}</plugin>
                <params>
                    <localPort>{args.port}</localPort>
                    <localIPAddress>{args.address}</localIPAddress>
                </params>
            </input>
        </inputPlugins>
        <outputPlugins>
            <output>
                <name>Overview output</name>
                <plugin>overview</plugin>
                <params>
                    <skipOptionsTemplates>true</skipOptionsTemplates>
                </params>
          </output>
        </outputPlugins>
    </ipfixcol2>
"""
    (tmpdir/"config.xml").write_text(config)
    procargs = ["ipfixcol2", "-c", str(tmpdir / "config.xml")]
    print(f"* Starting IPFIXcol2: {' '.join(procargs)}")
    proc = subprocess.Popen(procargs, stdout=open(tmpdir / "stdout", "w"), stderr=open(tmpdir / "stderr", "w"))

    print(f"* Waiting {args.interval} seconds", end="\r")
    for i in range(1, args.interval + 1):
        if proc.poll() is not None: # process exited
            break
        time.sleep(1)
        print(f"* Waiting {args.interval - i} seconds           ", end="\r")
    print()

    if proc.poll() is None: # process is still running
        print("* Stopping IPFIXcol2")
        proc.terminate()
        proc.wait()

    if proc.returncode != 0:
        print(f"{RED}ERROR: IPFIXcol2 process unexpectedly exited with code {proc.returncode}{RESET}.")
        print()
        print(f"{RED}=== Stderr log ================================================================={RESET}")
        print((tmpdir / "stderr").read_text())
        print(f"{RED}================================================================================{RESET}")
        print()
        sys.exit(1)

    try:
        data = json.loads((tmpdir / "stdout").read_text())
    except json.JSONDecodeError as e:
        print(f"{RED}ERROR: IPFIXcol2 produced unexpected data. Could not decode JSON: {e}{RESET}")
        print()
        print(f"{RED}=== Stdout log ================================================================={RESET}")
        print((tmpdir / "stdout").read_text())
        print(f"{RED}================================================================================{RESET}")
        print()
        print(f"{RED}=== Stderr log ================================================================={RESET}")
        print((tmpdir / "stderr").read_text())
        print(f"{RED}================================================================================{RESET}")
        print()
        sys.exit(1)

    if len(data["elements"]) == 0:
        print(RED)
        print(f"ERROR: No data received.")
        print()
        print("Ensure the specified port and connection type is correct and that the specified interval is long enough.")
        print("If unsure, try increasing the interval (-i).")
        if args.type == "udp" and args.interval < 600:
            print()
            print("In case of UDP, you might want to try to increase the interval up to 300 or even 600.")
        print(RESET)
        sys.exit(1)

    fields = []

    fields.append(Field(name="odid", data_type="UInt32", note="maps to the ODID the flow originated from", in_percent_of_records=100.0))

    for elem in data["elements"]:
        if elem["name"] is None:
            print(YELLOW, end="")
            print(f"WARNING: Element {elem['pen']}:{elem['id']} is missing definition - skipping field")
            print(RESET, end="")
            continue

        if elem["data_type"] not in IPFIX_TYPE_TO_CLICKHOUSE:
            print(YELLOW, end="")
            print(f"WARNING: Element {elem['name']}) has unsupported data type {elem['data_type']} - skipping field")
            print(RESET, end="")
            continue
        data_type = IPFIX_TYPE_TO_CLICKHOUSE[elem["data_type"]]

        name = elem["name"]
        alias_of = []
        for alias in elem["aliases"]:
            # exclude aliases with space in their name as those are inconvenient to use in SQL
            if is_exclusive_alias(data, alias) and " " not in alias:
                alias_of = [name]
                name = alias
                break

        fields.append(Field(name=name, data_type=data_type, alias_of=alias_of, in_percent_of_records=elem["in_percent_of_records"]))

    if ((have_elem(data, "iana:sourceIPv4Address") and have_elem(data, "iana:sourceIPv6Address"))
            or (have_elem(data, "iana:destinationIPv4Address") and have_elem(data, "iana:destinationIPv6Address"))):
        fields = replace_fields(
            fields,
            ["iana:sourceIPv4Address", "iana:sourceIPv6Address"],
            Field(name="srcip", data_type="IPv6", alias_of=["iana:sourceIPv4Address", "iana:sourceIPv6Address"])
        )
        fields = replace_fields(
            fields,
            ["iana:destinationIPv4Address", "iana:destinationIPv6Address"],
            Field(name="dstip", data_type="IPv6", alias_of=["iana:destinationIPv4Address", "iana:destinationIPv6Address"])
        )

    if (have_elem(data, "iana:flowStartSeconds")
            or have_elem(data, "iana:flowStartMilliseconds")
            or have_elem(data, "iana:flowStartMicroseconds")
            or have_elem(data, "iana:flowStartNanoseconds")):
        fields = replace_fields(
            fields,
            ["iana:flowStartSeconds", "iana:flowStartMilliseconds", "iana:flowStartMicroseconds", "iana:flowStartNanoseconds"],
            Field(name="flowstart", data_type="DateTime64(9)", alias_of=["iana:flowStartSeconds", "iana:flowStartMilliseconds", "iana:flowStartMicroseconds", "iana:flowStartNanoseconds"])
        )

    if (have_elem(data, "iana:flowEndSeconds")
            or have_elem(data, "iana:flowEndMilliseconds")
            or have_elem(data, "iana:flowEndMicroseconds")
            or have_elem(data, "iana:flowEndNanoseconds")):
        fields = replace_fields(
            fields,
            ["iana:flowEndSeconds", "iana:flowEndMilliseconds", "iana:flowEndMicroseconds", "iana:flowEndNanoseconds"],
            Field(name="flowend", data_type="DateTime64(9)", alias_of=["iana:flowEndSeconds", "iana:flowEndMilliseconds", "iana:flowEndMicroseconds", "iana:flowEndNanoseconds"])
        )

    config = generate_config(args, fields)
    schema = generate_schema(fields)

    print()
    print(f"{GREEN}=== Schema ====================================================================={RESET}")
    print(schema)
    print(f"{GREEN}================================================================================{RESET}")
    write_schema_file = not args.schema_file.exists() or args.overwrite or confirm_yesno(f"{args.schema_file} already exists. Overwrite?")
    if write_schema_file:
        args.schema_file.write_text(schema)
        print(f"Schema has been saved to: {args.schema_file}")
    print()
    print(f"{GREEN}=== Config ====================================================================={RESET}")
    print(config)
    print(f"{GREEN}================================================================================{RESET}")
    write_config_file = not args.config_file.exists() or args.overwrite or confirm_yesno(f'{args.config_file} already exists. Overwrite?')
    if write_config_file:
        args.config_file.write_text(config)
        print(f"Config has been saved to: {args.config_file}")
    print()

    print("You may use the generated schema and config as a starting point to run ipfixcol2 with a ClickHouse database.")
    print()
    print("- Execute the generated SQL schema to set-up a ClickHouse table that will be used to store the flows.")
    print("  e.g. $ clickhouse-client --host 127.0.0.1 --port 9000 --user ipfixcol2 --password ipfixcol2 < schema.sql")
    print()
    print("- Save the generated XML config to use with ipfixcol2. Make sure to change the example connection parameters.")
    print("  e.g. $ ipfixcol2 -c config.xml")
    print()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        # Do not print stack trace on Ctrl+C
        sys.exit(1)
