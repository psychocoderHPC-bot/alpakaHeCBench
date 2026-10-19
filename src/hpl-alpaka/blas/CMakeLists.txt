#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
# Standalone build of the alpakaVendor-based HPL BLAS shim. It is deliberately
# a separate CMake project: the root alpakaHeCBench build already pulls in
# extern/alpaka, and alpakaVendor brings its own (pinned) alpaka3. Keeping the
# two apart avoids target and option collisions.
#
cmake_minimum_required(VERSION 3.28)

project(hpl_alpaka_blas LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(HPL_ALPAKA_BACKEND "cpu" CACHE STRING "HPL BLAS backend: cpu, cuda, hip or sycl")
set_property(CACHE HPL_ALPAKA_BACKEND PROPERTY STRINGS cpu cuda hip sycl)
string(TOLOWER "${HPL_ALPAKA_BACKEND}" HPL_ALPAKA_BACKEND_LOWER)
if(NOT HPL_ALPAKA_BACKEND_LOWER MATCHES "^(cpu|cuda|hip|sycl|oneapi)$")
    message(FATAL_ERROR "HPL_ALPAKA_BACKEND must be cpu, cuda, hip or sycl (got '${HPL_ALPAKA_BACKEND}')")
endif()
if(HPL_ALPAKA_BACKEND_LOWER STREQUAL "oneapi")
    set(HPL_ALPAKA_BACKEND_LOWER "sycl")
endif()

# alpakaVendor options. FFT is not used here, so avoid its FFTW dependency.
set(alpakaV_TESTS OFF CACHE BOOL "" FORCE)
set(alpakaV_DOCS OFF CACHE BOOL "" FORCE)
set(alpakaV_DEP_FFTW OFF CACHE BOOL "" FORCE)
set(alpakaV_DEP_OPENBLAS ON CACHE BOOL "" FORCE)
set(alpakaV_DEP_CUFFT OFF CACHE BOOL "" FORCE)
set(alpakaV_DEP_ROCFFT OFF CACHE BOOL "" FORCE)

if(HPL_ALPAKA_BACKEND_LOWER STREQUAL "cuda")
    enable_language(CUDA)
    set(alpaka_DEP_CUDA ON CACHE BOOL "" FORCE)
    set(alpakaV_DEP_CUBLAS ON CACHE BOOL "" FORCE)
elseif(HPL_ALPAKA_BACKEND_LOWER STREQUAL "hip")
    enable_language(HIP)
    set(alpaka_DEP_HIP ON CACHE BOOL "" FORCE)
    set(alpakaV_DEP_ROCBLAS ON CACHE BOOL "" FORCE)
elseif(HPL_ALPAKA_BACKEND_LOWER STREQUAL "sycl")
    set(alpaka_DEP_ONEAPI ON CACHE BOOL "" FORCE)
    set(alpakaV_DEP_ONEMKL ON CACHE BOOL "" FORCE)
endif()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../extern/alpakaVendor" alpakaVendor)

set(HPL_ALPAKA_SOURCES hpl_alpaka.cpp)

add_library(hpl_alpaka SHARED ${HPL_ALPAKA_SOURCES})
target_link_libraries(hpl_alpaka PRIVATE alpakaVendor::alpakaVendor alpaka::alpaka)
alpaka_finalize(hpl_alpaka)

add_executable(hla_selftest selftest.cpp)
target_link_libraries(hla_selftest PRIVATE hpl_alpaka alpakaVendor::alpakaVendor alpaka::alpaka)
alpaka_finalize(hla_selftest)

if(HPL_ALPAKA_BACKEND_LOWER STREQUAL "cuda")
    set_source_files_properties(${HPL_ALPAKA_SOURCES} selftest.cpp PROPERTIES LANGUAGE CUDA)
elseif(HPL_ALPAKA_BACKEND_LOWER STREQUAL "hip")
    set_source_files_properties(${HPL_ALPAKA_SOURCES} selftest.cpp PROPERTIES LANGUAGE HIP)
elseif(HPL_ALPAKA_BACKEND_LOWER STREQUAL "sycl")
    target_compile_options(hpl_alpaka PRIVATE -fsycl)
    target_link_options(hpl_alpaka PRIVATE -fsycl)
    target_compile_options(hla_selftest PRIVATE -fsycl)
    target_link_options(hla_selftest PRIVATE -fsycl)
endif()
