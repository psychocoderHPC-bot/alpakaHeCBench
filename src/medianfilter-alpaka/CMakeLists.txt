#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME medianfilter
    MODEL alpaka
    SOURCES main.cpp helper.hpp misc.hpp
    CATEGORIES algorithms
    # Fast correctness subset from the source port. The upstream subset.json form is the
    # positional `<ppm image> <repeat>` pair; the image path is optional and ignored because
    # the port generates a deterministic RGBA image.
    TEST_ARGS --width 257 --height 255 --repeat 4 --samples 2
    # CTest uses CMake's regex engine, which has no non-capturing groups; this is the
    # CMake-compatible equivalent of the upstream Python regex
    # '(?:Average kernel execution time: )([0-9.+-e]+)(?: \(s\))'.
    TEST_REGEX "Average kernel execution time: [0-9.+-e]+ \\(s\\)"
    TEST_TIMEOUT 300
)
