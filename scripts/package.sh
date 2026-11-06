#!/usr/bin/env bash
# Builds the two release zips of one platform from the CMake artefacts folder:
#   ParallaxStyle-<version>-<platform>-vst3.zip        ParallaxStyle-<x_y_z>.vst3, install script
#   ParallaxStyle-<version>-<platform>-standalone.zip  ParallaxStyle-<version>[.exe|.app]
# Both contain the PDF guide and LEGGIMI.txt. The VST3 bundle keeps the version
# with underscores given by CMake (see CMakeLists.txt); the Standalone gets the
# dotted version.
#
# usage: package.sh <version> <linux|macos|windows> <artefacts dir> <guide pdf> <output dir>
set -euo pipefail

if [[ $# -ne 5 ]]; then
    echo "usage: $0 <version> <linux|macos|windows> <artefacts dir> <guide pdf> <output dir>" >&2
    exit 2
fi

VERSION="$1"; PLATFORM="$2"; ART="$3"; PDF="$4"; OUT="$5"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NAME="ParallaxStyle-$VERSION"                  # archives and Standalone
BUILT="ParallaxStyle-${VERSION//./_}"           # what CMake builds

case "$PLATFORM" in
    linux)   EXT="";     INSTALL="install.sh" ;;
    macos)   EXT=".app"; INSTALL="install.sh" ;;
    windows) EXT=".exe"; INSTALL="install.bat" ;;
    *) echo "unknown platform: $PLATFORM" >&2; exit 2 ;;
esac
APP="$NAME$EXT"

[[ -d "$ART/VST3/$BUILT.vst3" ]]       || { echo "missing $ART/VST3/$BUILT.vst3" >&2; exit 1; }
[[ -e "$ART/Standalone/$BUILT$EXT" ]]  || { echo "missing $ART/Standalone/$BUILT$EXT" >&2; exit 1; }
[[ -f "$PDF" ]]                 || { echo "missing guide $PDF" >&2; exit 1; }

mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

leggimi() {
    cat <<INFO
ParallaxStyle $VERSION - $1 per $PLATFORM

Contenuto:
$2
  $(basename "$PDF")   guida: installazione, avvio, aggiornamento e novità

Commit: ${GITHUB_SHA:-locale}
Build:  $(date -u +"%Y-%m-%d %H:%M") UTC
Sito:   https://github.com/Darkglassd3v/ParallaxStyle
INFO
}

# VST3
mkdir "$STAGE/vst3"
cp -R "$ART/VST3/$BUILT.vst3" "$STAGE/vst3/"
cp "$ROOT/packaging/$INSTALL" "$STAGE/vst3/"
chmod +x "$STAGE/vst3/$INSTALL"
cp "$PDF" "$STAGE/vst3/"
leggimi "plugin VST3" "  $BUILT.vst3   il plugin
  $INSTALL   installa o aggiorna il plugin (rimuove le versioni precedenti)" > "$STAGE/vst3/LEGGIMI.txt"
(cd "$STAGE/vst3" && cmake -E tar cf "$OUT/$NAME-$PLATFORM-vst3.zip" --format=zip -- *)

# Standalone
mkdir "$STAGE/app"
cp -R "$ART/Standalone/$BUILT$EXT" "$STAGE/app/$APP"
cp "$PDF" "$STAGE/app/"
leggimi "applicazione Standalone" "  $APP   il programma, si avvia senza DAW" > "$STAGE/app/LEGGIMI.txt"
(cd "$STAGE/app" && cmake -E tar cf "$OUT/$NAME-$PLATFORM-standalone.zip" --format=zip -- *)

echo "$OUT/$NAME-$PLATFORM-vst3.zip"
echo "$OUT/$NAME-$PLATFORM-standalone.zip"
