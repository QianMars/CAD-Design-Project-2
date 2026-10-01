#!/bin/sh
# Usage: sh scripts/make_submit.sh STUDENT_ID
# Produces ../STUDENT_ID.zip containing STUDENT_ID/{src,CMakeLists.txt,README.md} only.
# The report PDF (STUDENT_ID.pdf) must be uploaded separately, NOT inside the zip.
set -e
[ -n "$1" ] || { echo "usage: $0 STUDENT_ID"; exit 1; }
ID="$1"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
mkdir "$TMP/$ID"
cp -r "$ROOT/src" "$ROOT/CMakeLists.txt" "$ROOT/README.md" "$TMP/$ID/"
(cd "$TMP" && zip -qr "$ROOT/../$ID.zip" "$ID")
rm -rf "$TMP"
echo "created $(cd "$ROOT/.." && pwd)/$ID.zip"
