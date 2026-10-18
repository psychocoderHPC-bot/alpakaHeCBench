#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME gaussian
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES algorithms
    # Upstream subset.json: `-q -t -s 4096`.
    TEST_ARGS -q -t -s 4096
    # CTest uses CMake's regex engine, which has no non-capturing groups; this is the
    # CMake-compatible equivalent of the upstream Python regex
    # '(?:Device offloading time )([0-9.+-e]+)(?: \(us\))'.
    TEST_REGEX "Device offloading time [0-9.+-e]+ \\(us\\)"
    # N=4096 is an O(N^3) elimination. The default five samples plus the warm-up and the
    # verification run make the serial CPU test exceed the upstream 300 s budget.
    TEST_TIMEOUT 900
)
