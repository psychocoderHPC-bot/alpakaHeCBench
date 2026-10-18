#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
# Upstream run configuration from src/scripts/benchmarks/subset.json: an
# 8192x8192 image transformed 100 times per measured sample. CMake's regex
# engine has no non-capturing groups; this is the CMake-compatible form of the
# upstream Python regex.
add_hecbench_benchmark(
    NAME dct8x8
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES math
    TEST_ARGS 8192 8192 100
    TEST_REGEX "Average DCT8x8 kernel execution time [0-9.]+ \\(s\\)"
    TEST_TIMEOUT 300
)
