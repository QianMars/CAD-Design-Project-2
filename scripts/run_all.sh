#!/bin/sh
# Build, then run every inputs/inputXXX.cbi -> results/outputXXX.cbi and verify with check.py
set -e
cd "$(dirname "$0")/.."
mkdir -p build results
(cd build && cmake .. >/dev/null && make 2>&1 | tail -1)
for f in inputs/input*.cbi; do
  id=$(basename "$f" .cbi | sed 's/input//')
  echo "== $f"
  ./build/cbi "$f" "results/output$id.cbi"
  python3 scripts/check.py "$f" "results/output$id.cbi"
done
