#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME resize
    MODEL alpaka
    SOURCES main.cpp helper.hpp misc.hpp
    CATEGORIES algorithms
    # Upstream HeCBench has no subset.json or benchmarks.yaml test metadata for
    # resize, so this follows the test invocation of the source port: a small
    # odd-sized resize for all three pixel types and both kernels.
    TEST_ARGS --in-width 257 --in-height 129 --out-width 65 --out-height 33 --channels 8 --repeat 4
              --samples 2
    # CTest uses CMake's regex engine, which has no non-capturing groups.
    TEST_REGEX "resize [a-z0-9]+/(nearest|bilinear) verification PASSED"
    TEST_TIMEOUT 300
)
