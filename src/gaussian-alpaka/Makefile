# SPDX-FileCopyrightText: 2026 René Widera
# SPDX-License-Identifier: ISC
#===============================================================================
# alpaka version of the HeCBench `gaussian` benchmark.
#
# `make` configures and builds the top-level alpakaHeCBench project (target
# `gaussian-alpaka`) and copies the resulting binary (built as `gaussian`) to
# `./main`, the name the HeCBench tooling invokes from this directory.
#
# The build is Release by default: the HeCBench `autohecbench.py` harness calls
# plain `make`, and without an explicit build type CMake would emit an
# unoptimized binary (no -O3). Release matches the native reference builds.
#===============================================================================

CMAKE     ?= cmake
# Repository root: the bench directory lives at <root>/src/gaussian-alpaka.
ROOT      := $(abspath $(CURDIR)/../..)
BUILD_DIR ?= build
TARGET    ?= gaussian-alpaka
PROGRAM   ?= main

# CUDA target architecture passed to CMake (CMake form, e.g. 80 not sm_80).
# Override with `make CUDA_ARCH=90` or `make ARCH=90` for a different GPU.
# The value is ignored on a CPU-only build (CUDA is disabled by default).
ARCH      ?= 80
CUDA_ARCH ?= $(ARCH)

# Host SIMD/optimization flags. Defaults to the local micro-architecture; pass
# `make HOST_CXX_FLAGS=` for a portable binary. The binary then needs the same
# CPU features on the machine that runs it.
HOST_CXX_FLAGS ?= -march=native

# Forward user options to CMake, e.g. `make CMAKE_ARGS="-DalpakaHeCBench_ENABLE_CUDA=ON"`.
CMAKE_ARGS ?=

.PHONY: all build clean run

all: build

build:
	$(CMAKE) -S $(ROOT) -B $(BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_CUDA_ARCHITECTURES=$(CUDA_ARCH) \
		-DCMAKE_CXX_FLAGS="$(HOST_CXX_FLAGS)" \
		-DalpakaHeCBench_USE_SOURCE_TREE=ON $(CMAKE_ARGS)
	$(CMAKE) --build $(BUILD_DIR) --target $(TARGET) -j
	rm -f ./$(PROGRAM)
	cp $(BUILD_DIR)/bin/alpaka/gaussian ./$(PROGRAM)

clean:
	rm -rf $(PROGRAM) $(BUILD_DIR)

run: build
	./$(PROGRAM) -q -t -s 4096
