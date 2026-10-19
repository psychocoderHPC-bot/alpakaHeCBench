#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME stencil1d
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES algorithms
    TEST_ARGS --length 1021 --repeat 2
    # CTest uses CMake's regex engine, which has no non-capturing groups; this is the
    # CMake-compatible equivalent of the upstream Python regex in subset.json.
    TEST_REGEX "stencil1d kernel runtime \\(batch of [0-9]+\\): [0-9.]+ us/kernel"
    TEST_TIMEOUT 300
)
