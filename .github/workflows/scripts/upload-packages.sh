#!/bin/bash
set -eu
IFS=$'\n'
set +e; assets="$(gh release view "$TAG_NAME" | grep '^asset:' | cut -f2 | grep -F "${IMAGE_NAME//:/.}")"; set -e
files="$(find build/ \( -iname '*.rpm' -or -iname '*.deb' \) )"
for f in $files; do
    newf="$IMAGE_NAME-$(basename "$f")"
    mv "$f" "$newf"
    gh release upload --clobber "$TAG_NAME" "$newf"
    set +e; assets="$(grep -vF "${newf//:/.}" <<<"$assets")"; set -e
done
for asset in $assets; do
    gh release delete-asset "$TAG_NAME" "$asset"
done
