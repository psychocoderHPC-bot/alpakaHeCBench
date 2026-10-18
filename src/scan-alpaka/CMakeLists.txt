#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME scan
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES algorithms
    # Fast correctness subset from the source port. The benchmark prints the
    # final "scan verification PASSED" line only when every variant matched the
    # independent host reference exactly.
    TEST_ARGS --n 100000 --chunk 0 --repeat 2 --samples 2 --type int
    TEST_REGEX "scan verification PASSED"
    TEST_TIMEOUT 300
)
