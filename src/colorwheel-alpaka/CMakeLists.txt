#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME colorwheel
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES algorithms
    # Upstream HeCBench has no subset.json or benchmarks.yaml test metadata for
    # colorwheel, so this follows the test invocation of the source port:
    # `--size 256 --repeat 2 --samples 1`.
    TEST_ARGS --size 256 --repeat 2 --samples 1
    TEST_REGEX "colorwheel verification PASSED"
    TEST_TIMEOUT 300
)
