<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# cross (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of the
[ORNL/HeCBench](https://github.com/ORNL/HeCBench) `cross` benchmark. One C++20 translation
unit computes the 3D cross product `out = x1 x x2` for a dense `(numRows, 3)` tensor and is
compiled unchanged for the host CPU and for a CUDA GPU. There is no backend branch and no
native CUDA/HIP/OpenMP kernel.

## Provenance

- HeCBench `cross` reference: commit
  [`0a17ee47ca6072192101036d4bf292e6612f22e6`](https://github.com/ORNL/HeCBench/commit/0a17ee47ca6072192101036d4bf292e6612f22e6)
  (`src/cross-{cuda,hip,omp,sycl}/main.*`).
- Original alpaka port: PR #26 head
  [`b0222a156a674afdccc8413c8506eb767f7cbdbe`](https://github.com/psychocoderHPC-bot/alpaka3/commit/b0222a156a674afdccc8413c8506eb767f7cbdbe)
  (`benchmark/cross/src/main_alpaka.cpp`, `helper.hpp`).
- alpaka pin:
  [`b7d339d07056a9a2a6c4051cc3927157bc5f0d51`](https://github.com/alpaka-group/alpaka/commit/b7d339d07056a9a2a6c4051cc3927157bc5f0d51)
  (`dev_v3.x`), wired as the `extern/alpaka` submodule.

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

Before any stdout the benchmark compares the worst-case footprint of the requested size against the
memory the process may use: the smallest finite value of `/proc/meminfo` `MemAvailable`, `sysconf`
physical pages and the cgroup memory limit (cgroup v2 `memory.max` with the path from
`/proc/self/cgroup`, cgroup v1 `memory/memory.limit_in_bytes`). A request above that budget prints
one stderr line naming the binding limit, writes nothing to stdout and exits non-zero. This covers
the native positional path and the extended `--size` path; the cgroup term prevents a container
request larger than `memory.max` from passing and then being OOM-killed without a diagnostic.

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

The `make` wrapper configures a `Release` build (`-O3`, `-march=native`), builds the `cross-alpaka`
target and copies `bin/alpaka/cross` to `./main` for the HeCBench tooling. Override the CUDA
architecture with `make CUDA_ARCH=<n>` (default `80`) and drop the host SIMD flag with
`make HOST_CXX_FLAGS=`. The vendored `tools/hecbench` takes `-v` as a **global** option, before the
subcommand: `python3 tools/hecbench -v run cross --model alpaka`.

Fast math is **OFF** by default because the native HeCBench `cross` references use plain `-O3`
without fast-math. To match a fast-math reference pass `-DalpakaHeCBench_FAST_MATH=ON` or
`-Dalpaka_FAST_MATH=ON` at configure time (device-side flags only).

## Reproducible build and measurement

The `cpu-icpx`, `hip` and `sycl` presets in the repository-root `CMakePresets.json` build
this benchmark once per target. Each preset writes exactly one binary to
`build/<preset>/bin/alpaka/cross`. The `scripts/` directory drives the campaign:

- `scripts/build_all.sh` configures and builds `cpu-icpx`, `hip` and `sycl` into separate
  build trees.
- `scripts/run_bench.sh` runs one binary over the campaign sizes and dtypes through
  `scripts/measure.py` with the ORNL-matching scope.
- `scripts/measure.py` is the corrected harness (positional arguments are always passed,
  `--prog-args-mode extended` for the alpaka extended interface, serial flock, 13-column
  CSV). It is reconstructed from the campaign evidence, so re-verify it against the original
  harness on one known cell before trusting a new campaign.
- `scripts/one_binary_per_target.sh` prints the `sha256`, `ldd` and `.comment` provenance of
  each binary, which is the evidence that no binary is reused across backends.

Build and run on the campaign host:

```bash
git submodule update --init extern/alpaka
scripts/build_all.sh

# CPU icpx host OpenMP
scripts/run_bench.sh build/cpu-icpx/bin/alpaka/cross baseline-c1-cpu-icpx cross3
# AMD gfx1100
scripts/run_bench.sh build/hip/bin/alpaka/cross baseline-c1-hip cross3
# Intel Arc A770, FP32 only (the device has no fp64 aspect)
scripts/run_bench.sh build/sycl/bin/alpaka/cross baseline-c1-sycl cross3 sycl --fp32-only
```

The presets read these environment variables, which default to the campaign host layout:

| preset | variable | default |
| --- | --- | --- |
| `cpu-icpx`, `sycl` | `ALPAKA_HECBENCH_ICX` | `icpx` on `PATH` |
| `hip` | `ALPAKA_HECBENCH_ROCM_ROOT` | `/opt/rocm` |
| `sycl` | `ALPAKA_HECBENCH_ONEAPI_ROOT` | `/opt/intel/oneapi` |

The `hip` preset passes `-DCMAKE_PROJECT_INCLUDE=cmake/hip/hip_preload.cmake`. CMake 3.28
leaves the HIP compile rule unset when the alpaka subproject enables the HIP language, and
the preload file runs the language check before that point. The preset also pins
`--offload-arch=gfx1100` and uses `/opt/rocm/llvm/bin/clang++` directly, because the HIP
wrapper is rejected as `CMAKE_HIP_COMPILER`.

The `sycl` preset sets `-Dalpaka_HOST_Cpu=OFF`. Without it, icpx 2026.1.1 crashes in the SYCL
device pass on this source. It also enables `-DALPAKA_DISABLE_Host_Cpu`. Pin the device at
run time with `ONEAPI_DEVICE_SELECTOR=level_zero:gpu` so both the alpaka and ORNL binaries
JIT the Arc GPU and never the OpenCL CPU device.

## Fairness protocol

The comparison follows `results/RESULTS.md` and ORNL/HeCBench
[commit `0a17ee47ca6072192101036d4bf292e6612f22e6`](https://github.com/ORNL/HeCBench/commit/0a17ee47ca6072192101036d4bf292e6612f22e6):

- No fast math on either side. ORNL uses plain `-O3`; `alpakaHeCBench_FAST_MATH` stays OFF.
- CPU pairing is **icpx against icpx** with `-march=native` and `-qopenmp`. ORNL `cross-omp`
  is an offload benchmark, and `-qopenmp` lowers its target region to a threaded region, so
  both sides are true CPU-threaded kernels with matched compiler and flags. `g++` cannot
  consume `-qopenmp` and its ORNL build measures offload overhead, so g++ is not a valid CPU
  reference.
- One binary per target, each linking only its own runtime.
- Intel Arc A770 has no FP64. Intel FP64 is N/A; only FP32 is compared there.
- Both sides time `repeat` enqueues plus one synchronization and report the mean, with one
  discarded warm-up to remove one-time first-touch cost.

## Selected implementation

The PR ships the unchanged baseline `src/cross-alpaka/main.cpp` (C1): an AoS flat
`numRows * 3` buffer, `makeIdxMap` plus `worker::linearThreadsInGrid`, the `cross1`, `cross2`
and `cross3` kernels, and `chunkSize 256`. C1 wins the primary geomean at 0.886 over the 20
equally weighted (architecture, size, dtype) cases on CPU-icpx, AMD gfx1100 and Intel Arc A770
FP32. Three portable alternatives all lose: C2 SoA planes plus `SimdAlgo::concurrent` (0.734),
C3 SoA planes plus three `transform` calls (0.476), and C4 AoS plus a per-worker `Simd<T,4>`
gather (0.336). No portable variant wins everywhere. The ORNL reference stays faster on CPU
(0.83x) and Intel (0.96x) and reaches near parity on AMD (0.91x), so this is a "least
regression / best port" choice, not a speedup win.

See `results/RESULTS.md` for the per-case table, the per-architecture candidate comparison
with the rejection reason for each variant, the full regression list, the geomean ranking,
achieved bandwidth, the noise caveat and the C5 negative result.

## Methodology correction

`results/RESULTS.md` measures campaign2, which supersedes a first campaign with two defects.
The first campaign's harness passed `--prog-args "--dtype float"` in a way that replaced the
positional `<size> <repeat>`, so every Intel row ran the wrong size, and it had no in-process
warm-up, so the single-kernel C2 candidate absorbed a roughly 25 ms cold start. Campaign2
fixes both. The changes are recorded in
`results/data/campaign2_applied_diffs.diff`.
