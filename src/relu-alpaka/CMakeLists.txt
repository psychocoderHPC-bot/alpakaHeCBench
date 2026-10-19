#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME relu
    MODEL alpaka
    SOURCES main.cpp helper.hpp misc.hpp
    CATEGORIES algorithms
    # Short validation run; the default 10M/100 would dominate a CPU test.
    TEST_ARGS 4096 2
    # CMake-compatible equivalent of the subset.json regex for the four variant timings.
    TEST_REGEX "Average execution time of (ReluGrad|Relu)_impl[12] Kernel ?: [0-9.]+ \\(us\\)"
    TEST_TIMEOUT 300
)
