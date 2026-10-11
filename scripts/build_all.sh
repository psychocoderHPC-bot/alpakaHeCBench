#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 René Widera
# SPDX-License-Identifier: ISC
#
# Build the cross benchmark once per target, each into its own build tree.
# Targets: cpu-icpx (Intel icpx host OpenMP), hip (AMD gfx1100), sycl
# (Intel oneAPI, spir64, FP32-only device).
#
# One binary per target is a hard requirement of the fairness protocol: no
# binary is reused across backends. This script produces exactly
# build/<preset>/bin/alpaka/cross for each target.
#
# Environment knobs (defaults match the campaign host). The presets carry
# literal defaults; this script forwards an override as a -D cache variable so
# an unset variable never expands to an empty compiler path:
#   ALPAKA_HECBENCH_ICX          path to icpx   (default: icpx on PATH)
#   ALPAKA_HECBENCH_ROCM_ROOT    ROCm root      (default: /opt/rocm)
#   ALPAKA_HECBENCH_ONEAPI_ROOT  oneAPI root    (default: /opt/intel/oneapi)
#   AMD_ARCH                     HIP arch       (default: gfx1100)
#   JOBS                         build parallelism (default: nproc)
#
# The alpaka submodule must be present:
#   git submodule update --init extern/alpaka

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JOBS="${JOBS:-$(nproc)}"

ICX="${ALPAKA_HECBENCH_ICX:-$(command -v icpx || true)}"
ROCM_ROOT="${ALPAKA_HECBENCH_ROCM_ROOT:-/opt/rocm}"
ONEAPI_ROOT="${ALPAKA_HECBENCH_ONEAPI_ROOT:-/opt/intel/oneapi}"

if [[ ! -e "${ROOT}/extern/alpaka/CMakeLists.txt" ]]; then
    echo "error: extern/alpaka is empty; run 'git submodule update --init extern/alpaka'" >&2
    exit 1
fi

build_preset() {
    local preset="$1"
    shift
    echo "=== configure: ${preset} ==="
    cmake --preset "${preset}" -S "${ROOT}" -Dalpaka_COMPILE_PEDANTIC=ON "$@"
    echo "=== build: ${preset} ==="
    cmake --build "${ROOT}/build/${preset}" --target cross-alpaka -j "${JOBS}"
    echo "=== binary: ${ROOT}/build/${preset}/bin/alpaka/cross ==="
}

# gfx1100 is the campaign default; the preset reads CMAKE_HIP_ARCHITECTURES, so
# override it here when AMD_ARCH differs.
if [[ "${AMD_ARCH:-gfx1100}" != "gfx1100" ]]; then
    echo "note: AMD_ARCH=${AMD_ARCH} is not gfx1100; pass -DCMAKE_HIP_ARCHITECTURES on a manual configure"
fi

build_preset cpu-icpx -DCMAKE_CXX_COMPILER="${ICX:-icpx}"
build_preset hip \
    -DCMAKE_HIP_COMPILER="${ROCM_ROOT}/llvm/bin/clang++" \
    -DCMAKE_HIP_COMPILER_ROCM_ROOT="${ROCM_ROOT}"
build_preset sycl \
    -DCMAKE_CXX_COMPILER="${ICX:-icpx}" \
    -DCMAKE_PREFIX_PATH="${ONEAPI_ROOT}"

echo
echo "All target binaries:"
for p in cpu-icpx hip sycl; do
    printf '  %-9s %s\n' "${p}" "${ROOT}/build/${p}/bin/alpaka/cross"
done
