#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME haversine
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES algorithms
    # Fast correctness subset: a small irregular size plus a nonzero seed. The
    # binary exits non-zero when verification fails, so CTest checks correctness
    # through the exit code and the presence of the timing line.
    TEST_ARGS --cities 100000 --repeat 4 --samples 2 --seed 7
    TEST_REGEX "RESULT backend=.* median_us=[0-9.]+"
    TEST_TIMEOUT 300
)
