#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME jacobi
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES math simulation
    # CTest uses CMake's regex engine, which has no non-capturing groups; this is the
    # CMake-compatible equivalent of the upstream Python regex.
    TEST_REGEX "Total elapsed time: [0-9.+-e]+ seconds"
    # Small convergence case for CI. It exercises the full loop and the host
    # reference; the upstream harness uses the defaults from subset.json.
    TEST_ARGS --n 64 --max-iters 2000 --repeat 2
    TEST_TIMEOUT 300
)
