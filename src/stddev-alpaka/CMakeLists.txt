#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME stddev
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES algorithms
    # Fast correctness subset. The upstream benchmark metadata keeps the native
    # 65536 16384 100 arguments; that shape is too slow for a practical CTest case
    # on the serial CPU executor.
    TEST_ARGS --D 320 --N 97 --repeat 2 --samples 2
    # CTest uses CMake's regex engine, which has no non-capturing groups; this is the
    # CMake-compatible equivalent of the upstream HeCBench Python regex.
    TEST_REGEX "Average execution time of stddev kernels: [0-9.]+ \\(s\\)"
    TEST_TIMEOUT 300
)
