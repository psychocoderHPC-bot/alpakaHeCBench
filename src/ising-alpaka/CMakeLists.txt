#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME ising
    MODEL alpaka
    SOURCES main.cpp helper.hpp misc.hpp
    CATEGORIES algorithms
    # The upstream subset.json entry for ising has no arguments, so the reference
    # harness runs the 5120 x 5120 defaults. A small lattice exercises both the
    # packed checkerboard update and the independent host reference in CI.
    TEST_ARGS --lattice-n 64 --lattice-m 64 --nwarmup 10 --niters 50 --repeat 1
    # CTest uses CMake's regex engine, which has no non-capturing groups; this is the
    # CMake-compatible equivalent of the upstream subset.json regex
    # '(?:elapsed time: )([0-9.+-e]+)(?: sec)'.
    TEST_REGEX "elapsed time: [0-9.+-e]+ sec"
    TEST_TIMEOUT 300
)
