#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME boxfilter
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES algorithms
    # Upstream subset.json uses the positional form `<ppm image> <repeat>` with
    # repeat 10000. This port generates its image internally, so the image path is
    # optional and ignored. CTest uses the positional repeat form with a small
    # repeat count to keep the serial CPU run bounded.
    TEST_ARGS 4 --samples 2
    # CTest uses CMake's regex engine, which has no non-capturing groups; this is the
    # CMake-compatible equivalent of the upstream Python regex
    # '(?:Average kernel execution time )([0-9.+-e]+)(?: \(us\))'.
    TEST_REGEX "Average kernel execution time [0-9.+-e]+ us"
    TEST_TIMEOUT 300
)
