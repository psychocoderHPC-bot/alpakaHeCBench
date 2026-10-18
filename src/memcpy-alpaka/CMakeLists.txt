#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME memcpy
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES algorithms
    # Fast correctness subset from the source port. The benchmark prints the
    # final "Memcpy verification PASSED" line only when every mode and size
    # matched byte for byte.
    TEST_ARGS --sizes 64,1000,65536 --repeat 4 --samples 2
    TEST_REGEX "Memcpy verification PASSED"
    TEST_TIMEOUT 300
)
