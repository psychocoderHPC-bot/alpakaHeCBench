#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 René Widera
# SPDX-License-Identifier: ISC
#
# Tool-compat regression for the alpaka `cross` port.
#
# Builds src/cross-alpaka/main, then drives the vendored upstream tools exactly
# as a HeCBench checkout would:
#   1. python3 tools/hecbench run cross --model alpaka
#   2. python3 src/scripts/autohecbench.py --yes-prompt -o <tmp>/cross-alpaka.csv \
#          --repeat 3 cross-alpaka
# and finally asserts that the cross regex captures exactly six numeric values
# from the raw benchmark stdout and that their sum is a finite float.
#
# The script exits non-zero on any failure. Step 2 requires a real benchmark run:
# it fails when `tools/hecbench run` reports no benchmark, skips the binary, or
# returns without a numeric mean. Step 4 independently parses the binary output
# and the autohecbench CSV, so a skip cannot slip through even if the tool exits 0.
#
# It is meant to be run on a machine with a usable accelerator (hal/A30); the
# main benchmark arguments are taken from src/scripts/benchmarks/subset.json, so
# the runtime matches the upstream test configuration.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

PYTHON="${PYTHON:-python3}"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

echo "== [1/4] build src/cross-alpaka/main =="
make -C src/cross-alpaka

echo "== [2/4] python3 tools/hecbench run cross --model alpaka =="
HECBENCH_LOG="$WORKDIR/hecbench-run.log"
"$PYTHON" tools/hecbench run cross --model alpaka | tee "$HECBENCH_LOG"
# The vendored tool returns 0 even when it skips the binary. Require an explicit
# numeric result per benchmark line and reject a skip or an empty run.
"$PYTHON" - "$HECBENCH_LOG" <<'PYGUARD'
import math
import re
import sys

log_path = sys.argv[1]
with open(log_path) as f:
    lines = [line.rstrip("\n") for line in f]

if any("No benchmarks to run" in line for line in lines):
    sys.exit("FAIL: tools/hecbench reported no benchmarks to run")

result_lines = [line for line in lines if re.match(r"^\[\d+/\d+\] ", line)]
if not result_lines:
    sys.exit("FAIL: tools/hecbench produced no benchmark result line")

for line in result_lines:
    m = re.search(r"\.\.\. (.+)$", line)
    value = m.group(1).strip() if m else ""
    if value == "skipped":
        sys.exit(f"FAIL: tools/hecbench skipped the benchmark: {line}")
    try:
        number = float(value)
    except ValueError:
        sys.exit(f"FAIL: tools/hecbench did not report a numeric result: {line}")
    if not math.isfinite(number):
        sys.exit(f"FAIL: tools/hecbench reported a non-finite result: {line}")

print(f"OK: tools/hecbench ran {len(result_lines)} benchmark(s) with finite results")
PYGUARD

echo "== [3/4] python3 src/scripts/autohecbench.py --repeat 3 cross-alpaka =="
"$PYTHON" src/scripts/autohecbench.py --yes-prompt \
    -o "$WORKDIR/cross-alpaka.csv" --repeat 3 cross-alpaka

echo "== [4/4] assert regex captures six finite values =="
"$PYTHON" - "$ROOT/src/scripts/benchmarks/subset.json" "$ROOT/src/cross-alpaka/main" "$WORKDIR/cross-alpaka.csv" <<'PY'
import json
import math
import re
import subprocess
import sys

subset_path, binary, csv_path = sys.argv[1], sys.argv[2], sys.argv[3]

with open(subset_path) as f:
    subset = json.load(f)
regex, args = subset["cross"][0], subset["cross"][1]

proc = subprocess.run([binary] + [str(a) for a in args], capture_output=True, text=True, check=True)
values = re.findall(regex, proc.stdout)
if len(values) != 6:
    sys.exit(f"FAIL: expected 6 regex captures, got {len(values)}: {values}")
total = sum(float(v) for v in values)
if not math.isfinite(total):
    sys.exit(f"FAIL: sum is not finite: {total}")

with open(csv_path) as f:
    rows = [line.split(",") for line in f.read().splitlines() if line]
if not rows:
    sys.exit("FAIL: autohecbench produced no CSV rows")
for row in rows:
    for cell in row[1:]:
        cell = cell.strip()
        if cell and not math.isfinite(float(cell)):
            sys.exit(f"FAIL: non-finite autohecbench value {cell!r} in {row}")

print(f"OK: 6 captures, sum={total:.6f} us; autohecbench CSV rows={len(rows)}")
PY

echo "tool-compat: PASS"
