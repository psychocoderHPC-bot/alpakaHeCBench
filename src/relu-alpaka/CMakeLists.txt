#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME relu
    MODEL alpaka
    SOURCES main.cpp helper.hpp misc.hpp
    CATEGORIES algorithms
    # Short validation run from the source port; the default 10M/100 would dominate a CPU test.
    TEST_ARGS --size 4096 --repeat 2
    # CMake-compatible equivalent of the subset.json regex for the ported output.
    TEST_REGEX "Relu.* kernel runtime median: [0-9.]+ us"
    TEST_TIMEOUT 300
)
