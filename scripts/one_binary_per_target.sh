#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 René Widera
# SPDX-License-Identifier: ISC
#
# Print the provenance of one binary per target. This is the evidence that the
# campaign used a distinct binary per backend, not one binary reused across
# backends.
#
# For each binary it prints:
#   - sha256
#   - file type / architecture
#   - ldd (shared libraries; the backend runtime must be unambiguous)
#   - .comment (compiler provenance)
#
# Usage:
#   scripts/one_binary_per_target.sh [binary ...]
#
# With no arguments it uses the three preset build trees:
#   build/cpu-icpx/bin/alpaka/cross
#   build/hip/bin/alpaka/cross
#   build/sycl/bin/alpaka/cross
#
# ORNL reference binaries can be added explicitly, for example:
#   scripts/one_binary_per_target.sh /path/to/ORNL/src/cross-omp/main \
#       /path/to/ORNL/src/cross-hip/main /path/to/ORNL/src/cross-sycl/main

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ $# -gt 0 ]]; then
    bins=("$@")
else
    bins=(
        "${ROOT}/build/cpu-icpx/bin/alpaka/cross"
        "${ROOT}/build/hip/bin/alpaka/cross"
        "${ROOT}/build/sycl/bin/alpaka/cross"
    )
fi

for bin in "${bins[@]}"; do
    echo "================================================================"
    echo "binary: ${bin}"
    if [[ ! -x "${bin}" ]]; then
        echo "  MISSING (not built or not executable)"
        continue
    fi
    echo "  sha256:  $(sha256sum "${bin}" | awk '{print $1}')"
    echo "  file:    $(file -b "${bin}")"
    echo "  --- ldd ---"
    ldd "${bin}" 2>&1 | sed 's/^/  /'
    echo "  --- .comment (compiler provenance) ---"
    readelf -p .comment "${bin}" 2>/dev/null | sed 's/^/  /' || echo "  (no .comment)"
done
echo "================================================================"
