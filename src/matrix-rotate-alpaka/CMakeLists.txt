#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME matrix-rotate
    MODEL alpaka
    SOURCES main.cpp helper.hpp misc.hpp
    CATEGORIES algorithms
    TEST_ARGS 5000 100
    # CTest uses CMake's regex engine, which has no non-capturing groups and
    # treats parentheses literally. This matches the per-rotation median.
    TEST_REGEX "median kernel time: [0-9.]+ (us/rotation)"
    TEST_TIMEOUT 300
)
