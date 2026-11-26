#!/bin/bash
set -eux
PACKAGER=${PACKAGER:-"$(git config user.name) <$(git config user.email)>"}
WITH_UNIREC=${WITH_UNIREC:-0}
WITH_CLICKHOUSE=${WITH_CLICKHOUSE:-0}

case "$1" in
ol8)
    OL_VERSION_MAJOR=8
    ;;
ol9)
    OL_VERSION_MAJOR=9
    ;;
ol10)
    OL_VERSION_MAJOR=10
    ;;
*)
    echo "usage: $0 <ol8|ol9|ol10>"
    exit 1
    ;;
esac

cd "$(realpath "$(dirname "$0")")"
docker build -f assets/Dockerfile.ol_builder -t ipfixcol2-rpm-builder.ol${OL_VERSION_MAJOR} \
    --build-arg WITH_UNIREC="$WITH_UNIREC" --build-arg WITH_CLICKHOUSE="$WITH_CLICKHOUSE" \
    --build-arg OL_VERSION_MAJOR="$OL_VERSION_MAJOR" \
    assets/


cid=$(docker create -v "$(realpath ..)":/ipfixcol2-src -e PACKAGER="$PACKAGER" "ipfixcol2-rpm-builder.ol${OL_VERSION_MAJOR}")
trap 'docker rm -f -v "$cid" >/dev/null' EXIT

docker start -a "$cid"
mkdir -p rpms
docker cp "$cid":/out_rpms/. ./rpms/
{ set +x; } 2>/dev/null
echo
echo "RPMs are located in $(realpath ./rpms)"
