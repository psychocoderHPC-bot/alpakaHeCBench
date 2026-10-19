#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
# HPL is a make/MPI build and cannot use add_hecbench_benchmark(). This file
# only registers a custom target and a CTest test that both drive the Makefile
# wrapper in this directory.
#
add_custom_target(hpl-alpaka
    COMMAND ${CMAKE_COMMAND} -E env HPL_ALPAKA_BACKEND=${alpakaHeCBench_HPL_BACKEND}
            ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_CURRENT_SOURCE_DIR} all
    BYPRODUCTS
        ${CMAKE_CURRENT_SOURCE_DIR}/main
        ${CMAKE_CURRENT_SOURCE_DIR}/HPL.dat
    COMMENT "Building HPL 2.3 with the alpakaVendor BLAS shim (${alpakaHeCBench_HPL_BACKEND})"
    VERBATIM
)

add_test(
    NAME hpl-alpaka
    COMMAND ${CMAKE_COMMAND} -E env
            HPL_ALPAKA_BACKEND=${alpakaHeCBench_HPL_BACKEND}
            HPL_ALPAKA_DEVICE=${alpakaHeCBench_HPL_BACKEND}
            ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_CURRENT_SOURCE_DIR} ctest-run
)
set_tests_properties(hpl-alpaka PROPERTIES
    PASS_REGULAR_EXPRESSION "[0-9]+ tests completed and passed residual checks"
    FAIL_REGULAR_EXPRESSION "FAILED"
    TIMEOUT 900
    LABELS "alpaka;algorithms"
)
