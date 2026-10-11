<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# cross (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of the HeCBench
`cross` benchmark. One C++20 translation unit computes the 3D cross product
`out = x1 x x2` for a dense `(numRows, 3)` tensor and is compiled unchanged for the host CPU and
for a CUDA GPU. There is no backend branch and no native CUDA/HIP/OpenMP kernel.

## Provenance

- HeCBench `cross` reference: commit `0a17ee47ca6072192101036d4bf292e6612f22e6`
  (`src/cross-{cuda,hip,omp,sycl}/main.*`).
- Original alpaka port: PR #26 head `b0222a15` (`benchmark/cross/src/main_alpaka.cpp`, `helper.hpp`).
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as the `extern/alpaka`
  submodule.

## Computed quantities

Each row is a three-vector. For input rows `x1` and `x2`:

```text
out[0] = x1[1] * x2[2] - x1[2] * x2[1]
out[1] = x1[2] * x2[0] - x1[0] * x2[2]
out[2] = x1[0] * x2[1] - x1[1] * x2[0]
```

Three kernels cover the native variants. Each runs in its own timed pass:

| kernel | formulation | stride |
| --- | --- | --- |
| `cross1` | inline element reads (torch-like) | runtime |
| `cross2` | hoisted register loads | runtime |
| `cross3` | hoisted register loads | unit (compile time) |

## Command line

Primary interface, byte-compatible with the native binaries:

```text
./main <number of rows in a 2D tensor> <repeat>
```

Both arguments must be positive integers. Any other invocation prints the native usage string to
stdout and exits 1.

Extended mode is opt-in and writes its diagnostics to stderr only:

```text
./main --size <rows> --repeat <n> --dtype <float|double|both> --extended
```

## Output contract

stdout carries exactly ten lines in the default (`--dtype both`) run: one `=========== Data type is
FP32/FP64 ==========` header per data type, three `Average execution time of cross<N> kernel: %f
(us)` lines per data type (6 total) and one `PASS`/`FAIL` line per data type (2 total). A single-data
type run (`--dtype float` or `--dtype double`) carries five lines. The process exits non-zero when
validation fails, when a non-finite output appears, when the requested size cannot be allocated or on
invalid arguments.

`PASS` compares each kernel output against an independent double-precision reference computed from
the stored inputs. Tolerances: `1e-3 + 1e-4*|ref|` for FP32 and `1e-9` for FP64.

## Timing

Compatible mode matches the native scope: no warm-up, one `steady_clock` around the whole `repeat`
loop, a single wait at the end, and the reported value is the mean (`total_ns * 1e-3 / repeat`).
Extended mode adds one discarded warm-up and per-iteration median/min/max/stddev/mean, labeled
`[extended]` on stderr.

## Build and run

CPU (source-tree alpaka, OpenMP executor):

```bash
cmake --preset cpu -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu --target cross-alpaka -j
./build/cpu/bin/alpaka/cross 4096 2
```

Serial CPU:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target cross-alpaka -j
```

CUDA (A30 / sm_80):

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target cross-alpaka -j
./build/cuda/bin/alpaka/cross 10000000 100
```

The `make` wrapper configures, builds and copies `bin/alpaka/cross` to `./main` for the HeCBench
tooling.
