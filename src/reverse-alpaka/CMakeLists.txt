#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME reverse
    MODEL alpaka
    SOURCES main.cpp helper.hpp misc.hpp
    CATEGORIES algorithms
    # Native HeCBench positional form: <iterations>. The binary also accepts the
    # --iterations, --length, --repeat and --all-backends options. The short run
    # keeps the correctness check quick on a serial CPU. Campaign metadata uses
    # the HeCBench reference iteration count (100).
    TEST_ARGS 4
    # CTest uses CMake's regex engine, which has no non-capturing groups and
    # treats parentheses literally.
    TEST_REGEX "Total kernel execution time \\(median of [0-9]+\\): [0-9.]+ \\(s\\)"
    # A PASS regex overrides the exit status in CTest. Catch an explicit
    # validation failure too, so the timing line alone cannot pass the test.
    TEST_FAIL_REGEX "FAIL"
    TEST_TIMEOUT 300
)
