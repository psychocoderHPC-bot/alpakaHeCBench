#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME bilateral
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES algorithms
    # The registered test is validation only: it exercises all three reference
    # radii and the double-precision host reference on a small image, so it
    # stays fast on a CPU backend. The upstream measurement arguments
    # (2960x1440, repeat=1000) are for a full timed run.
    TEST_ARGS --width 128 --height 96 --verify-only
    TEST_REGEX "bilateral verification PASSED"
    TEST_TIMEOUT 300
)
