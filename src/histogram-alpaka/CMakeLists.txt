#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME histogram
    MODEL alpaka
    SOURCES main.cpp
    # Upstream subset.json geometry: --width=3840 --height=2160. The port accepts
    # the reference --width/--height spelling and derives the pixel count from it.
    TEST_ARGS --width=3840 --height=2160
    # CTest uses CMake's regex engine, which has no non-capturing groups; this is
    # the CMake-compatible equivalent of the upstream Python regex
    # '(?:Avg time )([0-9.+-e]+)(?: us )'.
    TEST_REGEX "Avg time [0-9.+-e]+ us "
    # The upstream geometry with the default 100 repetitions and 5 samples runs
    # three genres on the serial CPU executor; allow enough time for that.
    TEST_TIMEOUT 2400
    CATEGORIES algorithms
)
