#!/usr/bin/env bash
# Builds the PDF guide of a version from docs/ParallaxStyle.md: pandoc writes a
# self-contained HTML page, headless Chrome (or Edge) prints it to PDF.
# The guide must have a "## Novità della versione <version>" section.
#
# usage: build-guide.sh <version> <output.pdf>
# PANDOC and CHROME override the programs found on the PATH.
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 <version> <output.pdf>" >&2
    exit 2
fi

VERSION="$1"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DOC="$ROOT/docs/ParallaxStyle.md"

if ! grep -qE "^## Novità della versione ${VERSION//./\\.}[[:space:]]*$" "$DOC"; then
    echo "docs/ParallaxStyle.md has no '## Novità della versione $VERSION' section: write it before the release." >&2
    exit 1
fi

mkdir -p "$(dirname "$2")"
OUT="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
rm -f "$OUT"

PANDOC="${PANDOC:-pandoc}"
CHROME="${CHROME:-}"
if [[ -z "$CHROME" ]]; then
    for candidate in google-chrome google-chrome-stable chromium chromium-browser \
                     "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
                     "/c/Program Files/Google/Chrome/Application/chrome.exe" \
                     "/c/Program Files (x86)/Microsoft/Edge/Application/msedge.exe"; do
        if command -v "$candidate" > /dev/null 2>&1 || [[ -x "$candidate" ]]; then
            CHROME="$candidate"
            break
        fi
    done
fi
[[ -n "$CHROME" ]] || { echo "Chrome, Chromium or Edge not found (set CHROME)" >&2; exit 1; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

(cd "$ROOT/docs" && "$PANDOC" ParallaxStyle.md --standalone --embed-resources --fail-if-warnings --css guide.css \
    --metadata title="ParallaxStyle" --metadata subtitle="Guida della versione $VERSION" \
    --metadata lang=it --toc --toc-depth=2 -o "$WORK/guide.html")

# Windows (Git Bash): Chrome wants Windows paths
HTML="$WORK/guide.html"; PDF="$OUT"; PROFILE="$WORK/profile"
if command -v cygpath > /dev/null; then
    HTML="$(cygpath -m "$HTML")"; PDF="$(cygpath -w "$PDF")"; PROFILE="$(cygpath -w "$PROFILE")"
fi

"$CHROME" --headless=new --disable-gpu --no-sandbox --no-pdf-header-footer \
    --user-data-dir="$PROFILE" --print-to-pdf="$PDF" "file:///${HTML#/}" > "$WORK/chrome.log" 2>&1 &
PID=$!
for _ in $(seq 1 120); do                       # Chrome exits after printing: at most 2 minutes
    kill -0 "$PID" 2> /dev/null || break
    sleep 1
done
kill "$PID" 2> /dev/null || true
wait "$PID" 2> /dev/null || true

if [[ ! -s "$OUT" || "$(head -c 4 "$OUT")" != "%PDF" ]]; then
    cat "$WORK/chrome.log" >&2
    echo "Chrome did not write the PDF" >&2
    exit 1
fi
echo "$OUT"
