#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME laplace3d
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES algorithms
    # Upstream subset.json: `./main 128 128 128 100 1`
    # (Nx, Ny, Nz, iterations/repeat, verify flag).
    TEST_ARGS 128 128 128 100 1
    # CTest uses CMake's regex engine, which has no non-capturing groups; this is
    # the CMake-compatible equivalent of the upstream Python regex
    # '(?:Average kernel execution time: )([0-9.+-e]+)(?: \(s\))'.
    TEST_REGEX "Average kernel execution time: [0-9.+-e]+ \\(s\\)"
    TEST_TIMEOUT 300
)
