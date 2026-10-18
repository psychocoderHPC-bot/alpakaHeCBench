#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME affine
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES algorithms
    TEST_ARGS --width 127 --height 129 --repeat 4 --samples 2
    # CTest uses CMake's regex engine, which has no non-capturing groups.
    TEST_REGEX "RESULT backend=[^ ]+ size=[0-9]+x[0-9]+ median_us=[0-9.]+"
    TEST_TIMEOUT 300
)
