#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME heat2d
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES algorithms
    # Upstream subset.json: `./main 1024 1024 100` (Lx, Ly, iterations).
    TEST_ARGS 1024 1024 100
    # CTest uses CMake's regex engine, which has no non-capturing groups; this is
    # the CMake-compatible equivalent of the upstream Python regex
    # '([0-9.+-e]+)(?: usec/iter)'.
    TEST_REGEX "[0-9.+-e]+ usec/iter"
    TEST_TIMEOUT 300
)
