#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
cmake_minimum_required(VERSION 3.28)

project(alpakaHeCBench
    LANGUAGES CXX
    VERSION 0.1.0
    DESCRIPTION "HeCBench ports written with alpaka"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(alpakaHeCBench_ENABLE_CUDA "Enable the CUDA backend (onAcc + onHost in one translation unit)" OFF)
option(alpakaHeCBench_USE_SOURCE_TREE "Use alpaka from extern/alpaka instead of an installed package" ON)

# Fast-math passthrough. The native HeCBench cross references are built with
# plain -O3 and no fast-math, so the default is OFF to keep the comparison
# honest. Turning it ON maps to alpaka's own alpaka_FAST_MATH option, which
# injects device-side flags only (nvcc --use_fast_math; HIP/SYCL -ffast-math).
# Passing -Dalpaka_FAST_MATH=ON (or OFF) directly also works and is documented
# in the README.
option(alpakaHeCBench_FAST_MATH "Enable fast-math in device kernels (maps to alpaka_FAST_MATH; default OFF matches the native references)" OFF)
if(alpakaHeCBench_FAST_MATH)
    set(alpaka_FAST_MATH ON CACHE STRING "Enable fast-math in kernels" FORCE)
elseif(NOT DEFINED alpaka_FAST_MATH)
    set(alpaka_FAST_MATH OFF CACHE STRING "Enable fast-math in kernels" FORCE)
endif()

if(alpakaHeCBench_ENABLE_CUDA)
    enable_language(CUDA)
    set(CMAKE_CUDA_ARCHITECTURES 80 CACHE STRING "CUDA architectures to build for")
    set(alpaka_DEP_CUDA ON CACHE BOOL "" FORCE)
endif()

# Do not pull in alpaka's own test/example/documentation/benchmark subprojects.
set(alpaka_TESTS OFF CACHE BOOL "" FORCE)
set(alpaka_EXAMPLES OFF CACHE BOOL "" FORCE)
set(alpaka_DOCS OFF CACHE BOOL "" FORCE)
set(alpaka_BENCHMARKS OFF CACHE BOOL "" FORCE)

if(alpakaHeCBench_USE_SOURCE_TREE)
    add_subdirectory(extern/alpaka EXCLUDE_FROM_ALL)
else()
    find_package(alpaka REQUIRED)
endif()

enable_testing()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
include(BenchmarkMacros)

add_subdirectory(src/cross-alpaka)
add_subdirectory(src/haversine-alpaka)
