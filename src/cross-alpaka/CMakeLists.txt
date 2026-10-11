#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME cross
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES algorithms
    TEST_ARGS 10000000 100
    # CTest uses CMake's regex engine, which has no non-capturing groups; this is the
    # CMake-compatible equivalent of the upstream Python regex.
    TEST_REGEX "Average execution time of cross[0-9]+ kernel: [0-9.]+ \\(us\\)"
    TEST_TIMEOUT 300
)
