#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
# alpaka-aware benchmark registration for alpakaHeCBench.
#
# Mirrors the upstream HeCBench `add_hecbench_benchmark` interface (NAME/MODEL/
# SOURCES/CATEGORIES/TEST_ARGS/TEST_REGEX) but registers a single-source alpaka
# target. The model is `alpaka`; there is no per-vendor source tree.

function(add_hecbench_benchmark)
    set(options "")
    set(oneValueArgs NAME MODEL TEST_REGEX TEST_TIMEOUT)
    set(multiValueArgs SOURCES CATEGORIES COMPILE_OPTIONS LINK_LIBRARIES INCLUDE_DIRS TEST_ARGS)
    cmake_parse_arguments(BENCH "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT BENCH_NAME)
        message(FATAL_ERROR "add_hecbench_benchmark: NAME is required")
    endif()
    if(NOT BENCH_MODEL)
        message(FATAL_ERROR "add_hecbench_benchmark: MODEL is required")
    endif()
    if(NOT BENCH_SOURCES)
        message(FATAL_ERROR "add_hecbench_benchmark: SOURCES is required")
    endif()

    string(TOLOWER "${BENCH_MODEL}" BENCH_MODEL_LOWER)
    if(NOT BENCH_MODEL_LOWER STREQUAL "alpaka")
        message(FATAL_ERROR "add_hecbench_benchmark: MODEL '${BENCH_MODEL}' is not supported, expected 'alpaka'")
    endif()

    set(TARGET_NAME "${BENCH_NAME}-${BENCH_MODEL_LOWER}")

    add_executable(${TARGET_NAME} ${BENCH_SOURCES})
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

    set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME ${BENCH_NAME}
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${BENCH_MODEL_LOWER}"
    )

    if(BENCH_INCLUDE_DIRS)
        target_include_directories(${TARGET_NAME} PRIVATE ${BENCH_INCLUDE_DIRS})
    endif()
    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

    if(BENCH_COMPILE_OPTIONS)
        target_compile_options(${TARGET_NAME} PRIVATE ${BENCH_COMPILE_OPTIONS})
    endif()

    # The benchmark runs on the accelerator and the host in one translation unit.
    target_link_libraries(${TARGET_NAME} PRIVATE alpaka::alpaka)
    if(alpakaHeCBench_ENABLE_CUDA)
        target_link_libraries(${TARGET_NAME} PRIVATE alpaka::cuda)
    endif()
    if(BENCH_LINK_LIBRARIES)
        target_link_libraries(${TARGET_NAME} PRIVATE ${BENCH_LINK_LIBRARIES})
    endif()

    # Rewrite the source language and inject the alpaka finalize guards.
    alpaka_finalize(${TARGET_NAME})

    # Register the benchmark in a global list and per-category targets.
    get_property(ALL_BENCHMARKS GLOBAL PROPERTY HECBENCH_ALL_BENCHMARKS)
    list(APPEND ALL_BENCHMARKS ${TARGET_NAME})
    set_property(GLOBAL PROPERTY HECBENCH_ALL_BENCHMARKS "${ALL_BENCHMARKS}")

    foreach(CATEGORY ${BENCH_CATEGORIES})
        string(TOLOWER "${CATEGORY}" CATEGORY_LOWER)
        get_property(CAT_BENCHMARKS GLOBAL PROPERTY HECBENCH_CATEGORY_${CATEGORY_LOWER})
        list(APPEND CAT_BENCHMARKS ${TARGET_NAME})
        set_property(GLOBAL PROPERTY HECBENCH_CATEGORY_${CATEGORY_LOWER} "${CAT_BENCHMARKS}")

        if(NOT TARGET category-${CATEGORY_LOWER})
            add_custom_target(category-${CATEGORY_LOWER})
        endif()
        add_dependencies(category-${CATEGORY_LOWER} ${TARGET_NAME})
    endforeach()

    if(NOT TARGET ${BENCH_NAME}-all)
        add_custom_target(${BENCH_NAME}-all)
    endif()
    add_dependencies(${BENCH_NAME}-all ${TARGET_NAME})

    # Optional CTest registration using the upstream-style output regex.
    if(BENCH_TEST_REGEX)
        if(NOT BENCH_TEST_TIMEOUT)
            set(BENCH_TEST_TIMEOUT 300)
        endif()
        enable_testing()
        set(TEST_CMD $<TARGET_FILE:${TARGET_NAME}>)
        if(BENCH_TEST_ARGS)
            list(APPEND TEST_CMD ${BENCH_TEST_ARGS})
        endif()
        add_test(NAME ${TARGET_NAME} COMMAND ${TEST_CMD})
        set_tests_properties(${TARGET_NAME} PROPERTIES
            PASS_REGULAR_EXPRESSION "${BENCH_TEST_REGEX}"
            TIMEOUT ${BENCH_TEST_TIMEOUT}
            LABELS "${BENCH_MODEL_LOWER};${BENCH_CATEGORIES}"
        )
    endif()

    message(STATUS "Registered benchmark: ${TARGET_NAME} (target '${TARGET_NAME}', output '${CMAKE_BINARY_DIR}/bin/${BENCH_MODEL_LOWER}/${BENCH_NAME}')")
endfunction()
