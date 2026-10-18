#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME black-scholes
    MODEL alpaka
    SOURCES main.cpp helper.hpp misc.hpp
    CATEGORIES algorithms
    # Upstream positional repeat (subset.json args: ["100"]) plus the port's smaller option
    # stream so the CPU serial CTest run stays fast; the default 50,000,000 options would
    # dominate a host test.
    TEST_ARGS 100 --options 37000 --samples 2
    # CTest uses CMake's regex engine, which has no non-capturing groups; this is the
    # CMake-compatible equivalent of the upstream subset.json regex
    # '(?:Processing time on GPU: )([0-9.+-e]+)(?: \(ms\))'.
    TEST_REGEX "Processing time on GPU: [0-9.+-e]+ \\(ms\\)"
    TEST_TIMEOUT 300
)
