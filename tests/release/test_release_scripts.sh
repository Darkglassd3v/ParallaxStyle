#!/usr/bin/env bash
# Tests of the release scripts on Linux and macOS: version check, packaging,
# install script and PDF guide. Run by CTest (test "ReleaseScripts").
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
fail() { echo "FAIL: $*" >&2; exit 1; }

VERSION="$(bash "$ROOT/scripts/version.sh")"
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail "version.sh printed '$VERSION'"

# --- the tag must match the version in CMakeLists.txt
bash "$ROOT/scripts/version.sh" "v$VERSION" > /dev/null || fail "tag v$VERSION rejected"
if bash "$ROOT/scripts/version.sh" "v0.0.1" > /dev/null 2>&1; then fail "tag v0.0.1 accepted"; fi

# --- packaging: a fake Linux build, as the artefacts folder of CMake looks
V=9.9.9
ART="$WORK/artefacts"
mkdir -p "$ART/VST3/ParallaxStyle-$V.vst3/Contents/x86_64-linux" "$ART/Standalone"
echo binary > "$ART/VST3/ParallaxStyle-$V.vst3/Contents/x86_64-linux/ParallaxStyle-$V.so"
printf '#!/bin/sh\necho standalone\n' > "$ART/Standalone/ParallaxStyle-$V"
chmod +x "$ART/Standalone/ParallaxStyle-$V"
PDF="$WORK/ParallaxStyle-$V-Guida.pdf"
printf '%%PDF-1.4 test\n' > "$PDF"

bash "$ROOT/scripts/package.sh" "$V" linux "$ART" "$PDF" "$WORK/out"

VST3_ZIP="$WORK/out/ParallaxStyle-$V-linux-vst3.zip"
APP_ZIP="$WORK/out/ParallaxStyle-$V-linux-standalone.zip"
[[ -f "$VST3_ZIP" ]] || fail "missing $VST3_ZIP"
[[ -f "$APP_ZIP" ]]  || fail "missing $APP_ZIP"
for entry in "ParallaxStyle-$V.vst3/" "ParallaxStyle-$V-Guida.pdf" "install.sh" "LEGGIMI.txt"; do
    unzip -l "$VST3_ZIP" | grep -qF "$entry" || fail "$entry not in the VST3 zip"
done
for entry in "ParallaxStyle-$V" "ParallaxStyle-$V-Guida.pdf" "LEGGIMI.txt"; do
    unzip -l "$APP_ZIP" | grep -qF "$entry" || fail "$entry not in the Standalone zip"
done
grep -q "$V" <(unzip -p "$VST3_ZIP" LEGGIMI.txt) || fail "LEGGIMI.txt without the version"

# --- the Standalone keeps its executable bit
mkdir "$WORK/app" && (cd "$WORK/app" && unzip -q "$APP_ZIP")
[[ -x "$WORK/app/ParallaxStyle-$V" ]] || fail "Standalone not executable after unzip"

# --- install.sh replaces every older ParallaxStyle bundle and nothing else
HOME_DIR="$WORK/home"
mkdir -p "$HOME_DIR/.vst3/ParallaxStyle.vst3" "$HOME_DIR/.vst3/ParallaxStyle-1.8.0.vst3" "$HOME_DIR/.vst3/OtherPlugin.vst3"
mkdir "$WORK/vst3" && (cd "$WORK/vst3" && unzip -q "$VST3_ZIP")
HOME="$HOME_DIR" bash "$WORK/vst3/install.sh" > /dev/null
[[ -f "$HOME_DIR/.vst3/ParallaxStyle-$V.vst3/Contents/x86_64-linux/ParallaxStyle-$V.so" ]] || fail "bundle not installed"
[[ ! -e "$HOME_DIR/.vst3/ParallaxStyle.vst3" ]]       || fail "old unversioned bundle left installed"
[[ ! -e "$HOME_DIR/.vst3/ParallaxStyle-1.8.0.vst3" ]] || fail "old 1.8.0 bundle left installed"
[[ -d "$HOME_DIR/.vst3/OtherPlugin.vst3" ]]           || fail "another plugin was removed"

# --- the guide needs the 'Novità della versione' section of the version
if bash "$ROOT/scripts/build-guide.sh" 0.0.1 "$WORK/no.pdf" 2> "$WORK/err.txt"; then
    fail "guide built for a version without its Novità section"
fi
grep -q "Novità della versione 0.0.1" "$WORK/err.txt" || fail "no clear message for the missing section"

if command -v pandoc > /dev/null && { command -v google-chrome > /dev/null || command -v chromium > /dev/null \
                                      || [[ -x "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" ]]; }; then
    bash "$ROOT/scripts/build-guide.sh" "$VERSION" "$WORK/guide.pdf"
    [[ "$(head -c 4 "$WORK/guide.pdf")" == "%PDF" ]] || fail "the guide is not a PDF"
else
    echo "pandoc or Chrome missing: PDF rendering not checked here"
fi

echo "release scripts: OK"
