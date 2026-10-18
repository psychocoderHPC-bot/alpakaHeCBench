#
# Copyright 2026 René Widera
# SPDX-License-Identifier: ISC
#
# CMake 3.28 HIP enable workaround for the alpaka subproject.
#
# alpaka enables the HIP language from inside its own CMake logic. On CMake
# 3.28 that path can leave CMAKE_HIP_COMPILE_OBJECT unset, so the generate
# step fails. Preloading this file with -DCMAKE_PROJECT_INCLUDE runs the
# language check before the project configures and the HIP rules are then
# present.
#
# This file is passed with -DCMAKE_PROJECT_INCLUDE, so it runs once for the
# top-level project and again for the alpaka subproject. The guard keeps the
# enable_language call from repeating.

if(NOT DEFINED ALPAKA_HECBENCH_HIP_PRELOAD_DONE)
    set(ALPAKA_HECBENCH_HIP_PRELOAD_DONE TRUE)

    include(CheckLanguage)
    check_language(HIP)
    if(CMAKE_HIP_COMPILER)
        enable_language(HIP)
        # Pull in CMake's HIP compile/link rule variables that CMake 3.28 does
        # not populate on this path.
        include(CMakeHIPInformation OPTIONAL)
    endif()
endif()
