#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME softmax
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES math ml
    TEST_ARGS 100000 784 0 100
    # CTest uses CMake's regex engine, which has no non-capturing groups; this is the
    # CMake-compatible equivalent of the upstream subset.json regex.
    TEST_REGEX "Average kernel execution time: [0-9.]+ \\(ms\\)"
    TEST_TIMEOUT 300
)
