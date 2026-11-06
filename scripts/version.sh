#!/usr/bin/env bash
# Prints the version of ParallaxStyle, written only in CMakeLists.txt
# (project(ParallaxStyle VERSION x.y.z)). With a tag (v1.9.0) as argument it
# fails unless the tag is that version: a release never carries another number.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(sed -nE 's/^project\(ParallaxStyle VERSION ([0-9]+\.[0-9]+\.[0-9]+)\).*/\1/p' "$ROOT/CMakeLists.txt")"

if [[ -z "$VERSION" ]]; then
    echo "CMakeLists.txt has no 'project(ParallaxStyle VERSION x.y.z)' line" >&2
    exit 1
fi

if [[ $# -gt 0 && "${1#v}" != "$VERSION" ]]; then
    echo "The tag $1 is not the version of the project ($VERSION): no release." >&2
    exit 1
fi

echo "$VERSION"
