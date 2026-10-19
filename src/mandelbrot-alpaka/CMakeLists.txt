#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME mandelbrot
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES algorithms
    # Upstream subset.json: `./main 1000` (1000 measured repetitions).
    TEST_ARGS 1000
    # CTest uses CMake's regex engine, which has no non-capturing groups; this is
    # the CMake-compatible equivalent of the upstream Python regex
    # '(?:Average kernel execution time: )([0-9.+-e]+)(?: ms)'.
    TEST_REGEX "Average kernel execution time: [0-9.+-e]+ ms"
    # The upstream argument runs 1000 measured repetitions with the reference
    # defaults (1080 x 1920 x 100); the timeout matches the benchmark metadata.
    TEST_TIMEOUT 300
)
