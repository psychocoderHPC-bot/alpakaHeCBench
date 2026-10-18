#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME axpby
    MODEL alpaka
    SOURCES main.cpp
    INCLUDE_DIRS src
    CATEGORIES algorithms
    TEST_ARGS --tensors 4 --fixed-length 4093 --chunk-size 512 --repeat 2
    # CTest uses CMake's regex engine, which has no non-capturing groups; this is the
    # CMake-compatible equivalent of the subset.json Python regex.
    TEST_REGEX "axpby kernel runtime median: [0-9.]+ us"
    TEST_TIMEOUT 300
)
