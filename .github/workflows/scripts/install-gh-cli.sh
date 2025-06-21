#!/bin/bash
set -eu
cd "$(mktemp -d)"
curl -sL "$(curl -sL "https://api.github.com/repos/cli/cli/releases" | grep _linux_amd64.tar.gz | grep https | head -n1 | cut -d'"' -f4)" -o gh.tar.gz
tar xzf gh.tar.gz
find gh*/ -name gh -exec mv {} /usr/local/bin/ \;
