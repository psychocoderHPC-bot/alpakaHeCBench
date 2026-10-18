#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME morphology
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES algorithms
    # Fast correctness subset from the source port's CTest registration.
    TEST_ARGS --width 129 --height 127 --hsize 5 --vsize 5 --repeat 2 --samples 2
    # CTest uses CMake's regex engine, so this matches the port's verification line.
    TEST_REGEX "morphology [0-9]+x[0-9]+ hsize=[0-9]+ vsize=[0-9]+ verification PASSED"
    TEST_TIMEOUT 300
)

# Boundary case for the maximum supported structuring element and the per-thread deque capacity.
add_test(
    NAME morphology-alpaka-hsize256
    COMMAND $<TARGET_FILE:morphology-alpaka>
            --width 129 --height 127 --hsize 256 --vsize 256 --repeat 2 --samples 1 --seed 12345
)
set_tests_properties(
    morphology-alpaka-hsize256
    PROPERTIES
        PASS_REGULAR_EXPRESSION "morphology [0-9]+x[0-9]+ hsize=[0-9]+ vsize=[0-9]+ verification PASSED"
        TIMEOUT 300
        LABELS "alpaka;algorithms"
)
