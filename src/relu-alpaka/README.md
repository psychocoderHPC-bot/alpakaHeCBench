<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# relu (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of the HeCBench
`relu` benchmark. One C++20 translation unit is compiled unchanged for the host CPU and for a
CUDA GPU. There is no backend branch and no native CUDA/HIP/OpenMP kernel.

## What it computes

The benchmark runs the two kernels of the HeCBench `relu` program, which are derived from
TensorFlow:

- **ReluGrad (fp16)**: `(feature > 0) ? gradient : 0`, evaluated on binary16 bit patterns with
  alpaka's SIMD facilities.
- **Relu (packed int8x4)**: `max(value, 0)` applied independently to the four signed bytes of
  every `int32` word.

Data generation matches the HeCBench references: `std::mt19937` seeded with `19937`, the same
number and order of fp16 draws before the packed integers, and a fixed gradient of 1.

## Parameters

`--size <count>` and `--repeat <n>` select the number of elements and the measured iterations.
Defaults are `10000000` and `100`. The native HeCBench positional form `<count> <repeat>` is
accepted as well; named and positional arguments cannot be mixed. One warm-up iteration runs
before the measured iterations, and the reported value is the median of the samples.

## What the test validates

`helper.hpp` contains an independent host reference. ReluGrad is recomputed in double precision
and compared with a tolerance of `1e-3`; NaN and Inf results fail. The packed-int Relu output is
compared bit-exactly against a per-byte recomputation. Both checks print `PASSED` or `FAILED`, and
any mismatch makes the process exit non-zero.

The CTest entry runs `relu-alpaka --size 4096 --repeat 2` and requires the ReluGrad timing line in
stdout. A validation failure or a missing line fails the test.

## Provenance

- HeCBench `relu` reference: commit `0a17ee47ca6072192101036d4bf292e6612f22e6`
  (`src/relu-cuda/main.cu`, `src/relu-omp/main.cpp`, `src/relu-cuda/reference.h`).
- Original alpaka port: alpaka3 PR #25 head `e7212f88f3ae`
  (`benchmark/relu/src/main_alpaka.cpp`, `helper.hpp`, `misc.hpp`).
- alpaka pin: `b7d339d07056a9a2a6c4051cc3927157bc5f0d51` (`dev_v3.x`), wired as the
  `extern/alpaka` submodule.

The upstream CUDA and OpenMP sources are not vendored. They are TensorFlow code under the
Apache License 2.0.

## Build and run

CPU serial, Release:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target relu-alpaka -j
./build/cpu-serial/bin/alpaka/relu --size 4096 --repeat 2
```

The native positional form is equivalent:

```bash
./build/cpu-serial/bin/alpaka/relu 4096 2
```

CUDA, default `sm_80`:

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target relu-alpaka -j
./build/cuda/bin/alpaka/relu --size 4096 --repeat 2
```

## Test

```bash
ctest --test-dir build/cpu-serial -R relu-alpaka --output-on-failure
```

The vendored metadata in `src/scripts/benchmarks/subset.json` uses the same short arguments, so
the upstream tooling can run the port:

```bash
python3 tools/hecbench -v run relu --model alpaka
```
