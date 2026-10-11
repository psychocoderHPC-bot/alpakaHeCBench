#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 René Widera
# SPDX-License-Identifier: ISC
#
# Run one already-built cross binary over a list of sizes and dtypes with the
# ORNL-matching timing scope, through scripts/measure.py. Strictly serial: the
# harness flock guarantees one timed process at a time.
#
# ORNL-matching scope:
#   - one timed region around all `repeat` enqueues, one synchronization at the
#     end, mean reported by the binary itself;
#   - no fast math (a build-time property of the binary, not a runtime flag);
#   - CPU pinned to OMP_NUM_THREADS=64;
#   - sizes 100000, 1000000, 10000000, 10000003 with repeats 100, 100, 30, 100
#     (the campaign schedule);
#   - in-process F2 warm-up is compiled into the candidate binary (see
#     results/data/campaign2_applied_diffs.diff); this script also discards one
#     whole process run (--warmup 1) as the campaign did.
#
# The Intel Arc A770 has no FP64. For a sycl binary only FP32 is available;
# --fp32-only selects the alpaka extended interface with --dtype float, exactly
# as the campaign invoked the candidate sycl binaries.
#
# Usage:
#   scripts/run_bench.sh <binary> <impl-label> <role> [target] [output.csv] [--fp32-only]
#
# target is one of cpu-icpx, hip, sycl. When omitted it is inferred from the
# binary path (build/<target>/bin/alpaka/cross).
#
# Examples:
#   scripts/run_bench.sh build/cpu-icpx/bin/alpaka/cross cand-c3-cpu-icpx crossSoA
#   scripts/run_bench.sh build/hip/bin/alpaka/cross cand-c2-hip cross3
#   scripts/run_bench.sh build/sycl/bin/alpaka/cross cand-c2-sycl cross3 sycl --fp32-only

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MEASURE="${ROOT}/scripts/measure.py"

if [[ $# -lt 3 ]]; then
    echo "usage: $0 <binary> <impl-label> <role> [target] [output.csv] [--fp32-only]" >&2
    exit 2
fi

BINARY="$1"; shift
IMPL="$1"; shift
ROLE="$1"; shift

TARGET=""
OUTPUT="results.csv"
FP32_ONLY=0
for arg in "$@"; do
    case "${arg}" in
        --fp32-only) FP32_ONLY=1 ;;
        cpu-icpx|hip|sycl) TARGET="${arg}" ;;
        *) OUTPUT="${arg}" ;;
    esac
done

if [[ -z "${TARGET}" ]]; then
    case "${BINARY}" in
        */cpu-icpx/*) TARGET="cpu-icpx" ;;
        */hip/*)      TARGET="hip" ;;
        */sycl/*)     TARGET="sycl" ;;
        *) echo "error: cannot infer target from '${BINARY}'; pass cpu-icpx|hip|sycl" >&2; exit 2 ;;
    esac
fi

SIZES=(100000 1000000 10000000 10000003)
REPEATS=(100 100 30 100)

case "${TARGET}" in
    sycl)
        # Arc A770 has no fp64: FP32 only, through the extended interface.
        BASE_ARGS=(--env "ONEAPI_DEVICE_SELECTOR=level_zero:gpu" --prog-args-mode extended)
        DTYPES=("float")
        ;;
    hip)
        BASE_ARGS=(--env "ROCR_VISIBLE_DEVICES=0")
        DTYPES=("both")
        ;;
    cpu-icpx)
        BASE_ARGS=(--env "OMP_NUM_THREADS=64")
        DTYPES=("both")
        ;;
esac

for i in "${!SIZES[@]}"; do
    size="${SIZES[$i]}"
    repeat="${REPEATS[$i]}"
    for dtype in "${DTYPES[@]}"; do
        [[ "${FP32_ONLY}" == "1" ]] && dtype="float"
        echo "=== ${IMPL}[${ROLE}] target=${TARGET} size=${size} repeat=${repeat} dtype=${dtype} ==="
        args=(--binary "${BINARY}" --size "${size}" --repeat "${repeat}" \
              --dtype "${dtype}" --role "${ROLE}" --impl "${IMPL}" \
              --reps 5 --warmup 1 --output "${OUTPUT}")
        args+=("${BASE_ARGS[@]}")
        if [[ "${dtype}" == "float" && "${TARGET}" == "sycl" ]]; then
            args+=(--prog-args "--dtype float")
        fi
        python3 "${MEASURE}" "${args[@]}"
    done
done

echo "Results appended to ${OUTPUT}"
