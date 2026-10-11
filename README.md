<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# alpakaHeCBench

Standalone, alpaka-based re-implementation of selected
[ORNL/HeCBench](https://github.com/ORNL/HeCBench) benchmarks. Each benchmark is
a single C++20 source compiled unchanged for the host CPU and for a CUDA GPU
through [alpaka](https://github.com/alpaka-group/alpaka). This is not a git
mirror of HeCBench: it is an independent repository that ports benchmark
definitions and keeps the upstream benchmark contract (CLI, stdout, timing
scope, validation).

The upstream tools (`tools/hecbench`, `src/scripts/autohecbench.py`) are vendored
verbatim so the upstream harness runs directly against this tree.

## Source provenance

- Upstream HeCBench pinned at commit
  [`0a17ee47ca6072192101036d4bf292e6612f22e6`](https://github.com/ORNL/HeCBench/commit/0a17ee47ca6072192101036d4bf292e6612f22e6)
  (the benchmark definitions, the tools and the metadata).
- alpaka pinned at commit
  [`b7d339d07056a9a2a6c4051cc3927157bc5f0d51`](https://github.com/alpaka-group/alpaka/commit/b7d339d07056a9a2a6c4051cc3927157bc5f0d51)
  (`dev_v3.x`), wired as the `extern/alpaka` submodule.
- The port follows the HeCBench `cross` benchmark; its logic is re-derived, not
  vendored. The upstream license text is kept in `src/cross-alpaka/LICENSE`.

## Prerequisites

- CMake >= 3.28.
- Ninja (used by the CMake presets) or Make.
- A C++20 compiler (GCC 13, Clang 17 or newer).
- Optional: an NVIDIA CUDA toolkit for the `cuda` preset.

## Clone

The repository uses a git submodule for alpaka. Clone recursively:

```bash
git clone --recurse-submodules https://github.com/psychocoderHPC-bot/alpakaHeCBench
```

If you already cloned without the submodule contents:

```bash
git submodule update --init --recursive
```

Source-code archives (GitHub tarballs) do not include submodule contents; obtain
the alpaka tree separately with the `git submodule update --init --recursive`
command above.

## CPU quick start

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target cross-alpaka -j
./build/cpu-serial/bin/alpaka/cross 4096 2
```

`--preset cpu` selects the OpenMP block executor (`OMP_NUM_THREADS=64`). Both CPU
presets configure a `Release` build (`-O3`) with `-march=native` for host SIMD.
`-march=native` is a preset default, not a hard dependency: pass
`-DCMAKE_CXX_FLAGS="-march=x86-64-v3"` (or `-DCMAKE_CXX_FLAGS=`) on the configure
line to override it.

## CUDA quick start

Requires the CUDA toolkit. The default target architecture is `sm_80` (A30/A100);
set `-DCMAKE_CUDA_ARCHITECTURES=<n>` to the target GPU (for example `80` for an
A30, `90` for an H100) when it differs.

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target cross-alpaka -j
./build/cuda/bin/alpaka/cross 10000000 100
```

## Fast math

The native HeCBench `cross` references are built with plain `-O3` and no
fast-math, so the default here is **OFF** to keep the comparison honest. To match
a fast-math reference, pass either of:

```bash
-DalpakaHeCBench_FAST_MATH=ON      # this repository's convenience option
-Dalpaka_FAST_MATH=ON              # alpaka's own option, passed through directly
```

Either maps to alpaka's `alpaka_FAST_MATH`, which injects device-side flags only
(nvcc `--use_fast_math`, HIP/SYCL `-ffast-math`). For the CUDA backend
(`alpakaCuda.cmake`) these are CUDA-language generator expressions, so the host
C++ code is compiled with `CMAKE_CXX_FLAGS` as usual and never receives
fast-math. The OpenMP host executor has no fast-math branch.

## Portability caveat

`-march=native` (the CPU preset default) and a hardcoded
`CMAKE_CUDA_ARCHITECTURES` bake the build machine's instruction set and GPU
generation into the binary. Such a binary is not portable to a different CPU or
to a GPU with a lower compute capability. Use
`-DCMAKE_CXX_FLAGS=-march=x86-64-v3` and an explicit
`-DCMAKE_CUDA_ARCHITECTURES` list for redistributable builds.

## Benchmark coverage

| benchmark | native HeCBench models | this repo (alpaka) |
| --- | --- | --- |
| `cross` | cuda, hip, omp, sycl | alpaka (single source) |

## Reproducible cross campaign

`src/cross-alpaka/README.md` documents the `cpu-icpx`, `hip` and `sycl` presets and the
`scripts/` drivers (`build_all.sh`, `run_bench.sh`, `measure.py`,
`one_binary_per_target.sh`). `src/cross-alpaka/RESULTS.md` holds the corrected campaign2
measurements, the fairness protocol, the per-architecture candidate comparison and the
geomean ranking. The PR ships the unchanged baseline `src/cross-alpaka/main.cpp` (C1), the
primary geomean winner at 0.886 over four portable alternatives; the ORNL reference stays
ahead on CPU and Intel and near parity on AMD, so this is a least-regression port, not a
speedup win. The README also records the campaign1-to-campaign2 methodology correction.

## Native comparison

Build the native references in a pinned HeCBench checkout
([`0a17ee47ca6072192101036d4bf292e6612f22e6`](https://github.com/ORNL/HeCBench/commit/0a17ee47ca6072192101036d4bf292e6612f22e6)):

```bash
git clone https://github.com/ORNL/HeCBench
cd HeCBench && git checkout 0a17ee47ca6072192101036d4bf292e6612f22e6
make -C src/cross-cuda ARCH=sm_80          # CUDA
make -C src/cross-omp -f Makefile          # official ICPX offload build
```

Then compare the outputs. The recorded value is the sum of the six kernel
averages:

```bash
./src/cross-cuda/main 10000000 100
./build/cuda/bin/alpaka/cross 10000000 100
```

The upstream `cross-omp` build targets an Intel offload backend and does not run
on an NVIDIA-only host; use its host-OpenMP variant as the CPU reference.

Recorded ratios use `alpaka / native`; a value below 1.0 means alpaka is faster.
The comparison reported here was produced on the A30 validation host (NVIDIA A30,
sm_80, CUDA 13.4, g++ 13.3.0, AMD EPYC 7452). Each timed value is the mean over
the `repeat` loop, matched to the native timing scope (no warm-up, a single
synchronization at the end). The host-OpenMP build is a substitute for the
prescribed Intel offload reference and is therefore reported separately, not
folded into the ratio.

## The `make` wrapper and the upstream tools

`make -C src/cross-alpaka` configures a `Release` build (`-O3`, `-march=native`),
builds the `cross-alpaka` target and copies the binary to `./main`, which is the
name the HeCBench tooling invokes. The CUDA architecture defaults to `80`; pass
`make CUDA_ARCH=<n>` (or `ARCH=<n>`) to target another GPU, and
`make HOST_CXX_FLAGS=` to drop `-march=native`.

The vendored tools recognize the `alpaka` model through a one-token change:
`"alpaka"` was added to `MODELS` in `tools/hecbench` and
`tools/generate_metadata.py`, and to `cross.models` in `benchmarks.yaml`.
Discovery then maps `src/cross-alpaka` to name `cross`, model `alpaka`, binary
`main`. `src/scripts/benchmarks/subset.json` and
`src/scripts/autohecbench.py` need no change: the file keys benchmarks by name
and has no model field, so `autohecbench` reads the `cross` regex and arguments
directly.

```bash
make -C src/cross-alpaka
python3 tools/hecbench -v run cross --model alpaka
python3 src/scripts/autohecbench.py --yes-prompt -o cross-alpaka.csv --repeat 3 cross-alpaka
```

`-v` is a **global** option and must precede the subcommand:
`python3 tools/hecbench -v run cross --model alpaka`. The form
`python3 tools/hecbench run cross --model alpaka -v` is rejected by argparse
(`unrecognized arguments: -v`, exit 2).

Run the regression script that exercises both tools and asserts six numeric
captures with a finite sum:

```bash
tests/tool-compat/run.sh
```

## How to update the alpaka pin

```bash
cd extern/alpaka
git fetch origin
git checkout <new-sha>
cd ../..
git add extern/alpaka
git commit -m "Update alpaka pin to <new-sha>"
```

Keep `branch = dev_v3.x` in `.gitmodules`; only the gitlink commit changes.
`extern/alpaka` is an external submodule and is not linted by REUSE.

## License

The top-level `LICENSE` is BSD-3-Clause. See `REUSE.toml` and `LICENSES/` for
the per-file license mapping.
