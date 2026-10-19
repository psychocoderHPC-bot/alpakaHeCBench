<!--
SPDX-FileCopyrightText: 2026 René Widera
SPDX-License-Identifier: ISC
-->

# HPL 2.3 on alpakaVendor BLAS

This directory contains HPL 2.3 from ORNL/HeCBench, built against a small
BLAS shim that routes the dominant `DGEMM` calls through
[alpakaVendor](https://github.com/psychocoderHPC-bot/alpakaVendor). The shim
selects the backend at runtime with `HPL_ALPAKA_DEVICE`:

| value | backend |
| --- | --- |
| `cpu` | OpenBLAS through the alpakaVendor host BLAS path |
| `cuda` | cuBLAS through alpakaVendor |
| `hip` | rocBLAS through alpakaVendor |
| `sycl` | oneMKL through alpakaVendor |

`HPL_ALPAKA_DEVICE` is required and must be one of `cpu`, `cuda`, `hip`, or
`sycl`. There is no fallback. A missing value, an unknown value, a backend
that is not enabled in the build, or a backend with no available device prints
a clear error and exits non-zero. The `cpu` build always has a host device.

## Provenance

- HPL 2.3 tree copied from
  [ORNL/HeCBench](https://github.com/ORNL/HeCBench) at commit
  `fd01f79b`, directory `src/hpl-cuda/src/hpl-2.3` (HPL 2.3, BSD-4-Clause; see
  `LICENSE.md`). The backend-specific `src/hpl-2.3/src/cuda/` shim was removed
  and replaced by `blas/hpl_alpaka.cpp`.
- alpakaVendor pinned at
  `ccf128e3c1ff85ae0b1fc5d9b08fd9bc44f02aeb` (`dev`), wired as
  `extern/alpakaVendor` with its nested alpaka3 submodule.
- The Q1 2026 CBLAS macro mapping in `include/hpl_blas.h`
  (`HPL_dgemm` -> `dpcpp_dgemm`, `HPL_dtrsm` -> `dpcpp_dtrsm`) is kept from
  the upstream tree.

## BLAS shim

The GEMM path is a direct translation of ORNL's `cuda_dgemm.cpp`. Each call
performs per-call blocking `alpaka::onHost::alloc`, wraps the HPL host
pointers in zero-copy `SharedBuffer` views, uploads A, B, and C, runs the
vendor GEMM, downloads C, and frees the buffers. It keeps the same host
fallback thresholds (`N < 64`, `M < 128`, or `K < 128`), the same C upload,
and the same three synchronization points as ORNL. `alpaka::onHost::alloc` is
the RAII, blocking equivalent of `cudaMalloc`/`cudaFree`. There is no buffer
cache and no skipped copy.

The DTRSM path uses alpakaVendor's host backend with a host device and host
queue. It does not call CBLAS directly. The dominant HPL work is GEMM; moving
TRSM to the accelerator is a documented follow-up.

## Requirements

- MPI (OpenMPI or MPICH; `mpicc` and `mpirun` on `PATH`).
- OpenBLAS with development headers and `openblas.pc`.
- CMake >= 3.28 and a C++20 compiler.
- For `cuda`: the CUDA toolkit and cuBLAS.
- For `hip`: ROCm with rocBLAS.
- For `sycl`: Intel oneAPI with oneMKL and `icpx`.

Initialize the submodules once:

```bash
git submodule update --init --recursive
```

## Build and run

CPU:

```bash
make -C src/hpl-alpaka BACKEND=cpu
make -C src/hpl-alpaka BACKEND=cpu run
```

`make` builds `blas/build/libhpl_alpaka.so`, then HPL, and copies `xhpl` to
`src/hpl-alpaka/main`; the small input deck is copied to `HPL.dat` in the same
directory. HPL reads `HPL.dat` from its working directory.

CUDA:

```bash
make -C src/hpl-alpaka BACKEND=cuda \
  CMAKE_ARGS="-DCMAKE_CUDA_ARCHITECTURES=80"
make -C src/hpl-alpaka BACKEND=cuda run
```

HIP:

```bash
make -C src/hpl-alpaka BACKEND=hip \
  CMAKE_ARGS="-DCMAKE_HIP_ARCHITECTURES=gfx1100"
make -C src/hpl-alpaka BACKEND=hip run
```

SYCL:

```bash
make -C src/hpl-alpaka BACKEND=sycl \
  CMAKE_ARGS="-DCMAKE_CXX_COMPILER=<icpx> -DIntelSYCL_DIR=<...> -DMKL_DIR=<...> -DTBB_DIR=<...>"
make -C src/hpl-alpaka BACKEND=sycl run
```

The cache variables point at the matching oneAPI components; `<icpx>` is the
Intel oneAPI C++ compiler, and the three `*_DIR` values select the Intel SYCL,
oneMKL and oneTBB package configurations.

Each `run` target exports `HPL_ALPAKA_DEVICE=$(BACKEND)` and launches one MPI
rank. Use `MPI_RANKS=<n>` to change the rank count and `HPL_DATA=<path>` to
select another input deck.

## Validation

The wrapper ships a numeric self-test that compares `dpcpp_dgemm` against the
CBLAS reference for N/N, N/T, T/N and T/T on padded leading dimensions:

```bash
HPL_ALPAKA_DEVICE=cpu ./src/hpl-alpaka/blas/build/hla_selftest
```

The self-test fails on a maximum relative error above `1e-10`.

Run the self-test and a one-rank HPL validation with the 1024x1024 input deck:

```bash
make -C src/hpl-alpaka BACKEND=cpu ctest-run
```

Two input decks are provided:

- `datafiles/HPL_alpaka_1024.dat`: N=1024, NB=128, P=Q=1, threshold 16.0,
  full PFACT/RFACT/BCAST sweep.
- `datafiles/HPL_alpaka_2048.dat`: the same deck with N=2048.

Both stay well below 1 GB of matrix storage on one rank.

## Known SYCL limitation

The wrapper builds against oneAPI, but `HPL_ALPAKA_DEVICE=sycl` cannot select
the Intel GPU on the tested setup and the shim exits with an error. Cause:
oneMKL BLAS headers register 2,388 SYCL kernel ids without device images, so
alpaka's SYCL platform check discards the Intel GPU when it cannot build a
matching kernel bundle. This is an upstream alpaka/oneMKL interaction, not an
HPL numeric failure. A direct oneMKL path and a patched or newer alpaka are
follow-ups.
