# SPDX-FileCopyrightText: 2026 René Widera
# SPDX-License-Identifier: ISC
#===============================================================================
# alpaka version of the HeCBench `cross` benchmark.
#
# `make` configures and builds the top-level alpakaHeCBench project (target
# `cross-alpaka`) and copies the resulting binary (built as `cross`) to `./main`,
# the name the HeCBench tooling invokes from this directory.
#===============================================================================

CMAKE     ?= cmake
# Repository root: the bench directory lives at <root>/src/cross-alpaka.
ROOT      := $(abspath $(CURDIR)/../..)
BUILD_DIR ?= build
TARGET    ?= cross-alpaka
PROGRAM   ?= main

# Forward user options to CMake, e.g. `make CMAKE_ARGS="-DalpakaHeCBench_ENABLE_CUDA=ON"`.
CMAKE_ARGS ?=

.PHONY: all build clean run

all: build

build:
	$(CMAKE) -S $(ROOT) -B $(BUILD_DIR) -DalpakaHeCBench_USE_SOURCE_TREE=ON $(CMAKE_ARGS)
	$(CMAKE) --build $(BUILD_DIR) --target $(TARGET) -j
	cp $(BUILD_DIR)/bin/alpaka/cross ./$(PROGRAM)

clean:
	rm -rf $(PROGRAM) $(BUILD_DIR)

run: build
	./$(PROGRAM) 10000000 100
