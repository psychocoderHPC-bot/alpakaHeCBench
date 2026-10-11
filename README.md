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

`--preset cpu` selects the OpenMP block executor (`OMP_NUM_THREADS=64`).

## CUDA quick start

Requires the CUDA toolkit. The default target architecture is `sm_80` (A30/A100).

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target cross-alpaka -j
./build/cuda/bin/alpaka/cross 10000000 100
```

## Benchmark coverage

| benchmark | native HeCBench models | this repo (alpaka) |
| --- | --- | --- |
| `cross` | cuda, hip, omp, sycl | alpaka (single source) |

## Native comparison

Build the native references in a pinned HeCBench checkout (`0a17ee47`):

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

The canonical native CUDA output on an A30 (sm_80) is:

```
=========== Data type is FP32 ==========
Average execution time of cross1 kernel: 433.417389 (us)
Average execution time of cross2 kernel: 430.737000 (us)
Average execution time of cross3 kernel: 430.518677 (us)
PASS
=========== Data type is FP64 ==========
Average execution time of cross1 kernel: 919.546692 (us)
Average execution time of cross2 kernel: 887.451172 (us)
Average execution time of cross3 kernel: 889.599243 (us)
PASS
```

The upstream `cross-omp` build targets an Intel offload backend and does not run
on an NVIDIA-only host; use its host-OpenMP variant as the CPU reference.

## Upstream tool compatibility

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
python3 tools/hecbench run cross --model alpaka
python3 src/scripts/autohecbench.py --yes-prompt -o cross-alpaka.csv --repeat 3 cross-alpaka
```

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
