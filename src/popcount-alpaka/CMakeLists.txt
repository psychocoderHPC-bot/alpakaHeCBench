#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
add_hecbench_benchmark(
    NAME popcount
    MODEL alpaka
    SOURCES main.cpp
    CATEGORIES algorithms
    # Native HeCBench positional form: <count> <repeat>. The binary also accepts
    # the --size and --repeat options. The short run keeps the correctness check
    # below a second on a serial CPU.
    TEST_ARGS 4096 2
    TEST_REGEX "Average kernel execution time \\(pc[1-6]\\): [0-9.]+ \\(us\\)"
    TEST_TIMEOUT 300
)
