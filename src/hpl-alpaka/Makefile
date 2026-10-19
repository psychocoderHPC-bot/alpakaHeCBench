# SPDX-FileCopyrightText: 2026 René Widera
# SPDX-License-Identifier: ISC
#===============================================================================
# HPL 2.3 on alpakaVendor BLAS.
#
# `make` builds the alpakaVendor BLAS shim (separate CMake project), then the
# HPL 2.3 library and xhpl binary, and copies xhpl to ./main, the name the
# HeCBench tooling invokes from this directory.  HPL reads HPL.dat from the
# current working directory.
#
# Backend selection:
#   make BACKEND=cpu     host OpenBLAS through alpakaVendor (default)
#   make BACKEND=cuda    cuBLAS through alpakaVendor
#   make BACKEND=hip     rocBLAS through alpakaVendor
#   make BACKEND=sycl    oneMKL through alpakaVendor
#===============================================================================

CMAKE       ?= cmake
BACKEND     ?= $(if $(HPL_ALPAKA_BACKEND),$(HPL_ALPAKA_BACKEND),cpu)
BLAS_BUILD  ?= $(CURDIR)/blas/build
HPL_DIR     := $(CURDIR)/src/hpl-2.3
HPL_DATA    ?= $(CURDIR)/datafiles/HPL_alpaka_1024.dat
MPIEXEC     ?= mpirun
MPI_RANKS   ?= 1
JOBS        ?= $(shell nproc)
CMAKE_ARGS  ?=

# Passed to the HPL makefiles. Keep it absolute: the HPL sub-makes run in
# nested directories.
export BLAS_BUILD

.PHONY: all blas hpl run ctest-run clean distclean

all: hpl

blas:
	$(CMAKE) -S $(CURDIR)/blas -B $(BLAS_BUILD) \
		-DCMAKE_BUILD_TYPE=Release \
		-DHPL_ALPAKA_BACKEND=$(BACKEND) $(CMAKE_ARGS)
	$(CMAKE) --build $(BLAS_BUILD) -j $(JOBS)

hpl: blas
	$(MAKE) -C $(HPL_DIR) clean
	cp $(HPL_DATA) $(HPL_DIR)/HPL.dat
	$(MAKE) -C $(HPL_DIR)
	cp $(HPL_DIR)/bin/intel64/xhpl $(CURDIR)/main
	cp $(HPL_DATA) $(CURDIR)/HPL.dat

run: all
	cd $(CURDIR) && HPL_ALPAKA_DEVICE=$(BACKEND) $(MPIEXEC) -n $(MPI_RANKS) ./main

# Self-test first, then a one-rank HPL run on the small input deck.
ctest-run: all
	HPL_ALPAKA_DEVICE=$(BACKEND) HPL_ALPAKA_MIN_GEMM=0 $(BLAS_BUILD)/hla_selftest
	cd $(CURDIR) && HPL_ALPAKA_DEVICE=$(BACKEND) $(MPIEXEC) -n $(MPI_RANKS) ./main

clean:
	$(MAKE) -C $(HPL_DIR) clean
	rm -f $(HPL_DIR)/HPL.dat $(CURDIR)/main $(CURDIR)/HPL.dat

distclean: clean
	rm -rf $(BLAS_BUILD)
