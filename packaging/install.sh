#!/usr/bin/env bash
# ParallaxStyle: installs or updates the VST3 plugin for the current user
# (macOS and Linux). Every previous ParallaxStyle bundle is removed first, so
# the DAW finds one version only. Usage: bash install.sh
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
case "$(uname -s)" in
    Darwin) DEST="$HOME/Library/Audio/Plug-Ins/VST3" ;;
    *)      DEST="$HOME/.vst3" ;;
esac

shopt -s nullglob
BUNDLES=("$HERE"/ParallaxStyle-*.vst3)
if [[ ${#BUNDLES[@]} -ne 1 ]]; then
    echo "Accanto a questo script deve esserci un solo ParallaxStyle-<versione>.vst3." >&2
    exit 1
fi
BUNDLE="$(basename "${BUNDLES[0]}")"

mkdir -p "$DEST"
rm -rf "$DEST"/ParallaxStyle.vst3 "$DEST"/ParallaxStyle-*.vst3
cp -R "${BUNDLES[0]}" "$DEST/"
if [[ "$(uname -s)" == Darwin ]]; then
    xattr -cr "$DEST/$BUNDLE" 2> /dev/null || true   # file scaricato: togli la quarantena
fi

echo "Installato: $DEST/$BUNDLE"
echo "Riavvia la DAW e fai una nuova scansione dei plugin."
