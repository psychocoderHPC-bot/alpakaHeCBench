#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME upsample
    MODEL alpaka
    SOURCES main.cpp helper.hpp misc.hpp
    CATEGORIES algorithms
    # Fast correctness subset from the source port. It exercises odd spatial sizes (31x33) in
    # addition to the default 64x64; the upstream `run` target uses `32 64 64 64 100`.
    TEST_ARGS --batch 4 --channels 8 --height 31 --width 33 --repeat 4 --samples 2
    # The port reports the median per-pass time in microseconds on the `RESULT` line.
    TEST_REGEX "RESULT .* median_us=[0-9.+-e]+"
    TEST_TIMEOUT 300
)
