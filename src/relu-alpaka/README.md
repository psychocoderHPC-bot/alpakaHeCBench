<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# relu (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of the HeCBench
`relu` benchmark. One C++20 translation unit is compiled unchanged for the host CPU and for a
CUDA GPU. There is no backend branch and no native CUDA/HIP/OpenMP kernel.

## What it computes

The benchmark runs the four variants of the HeCBench `relu` program, which are derived from
TensorFlow. All four run as alpaka kernels on the same input size and repeat count:

| variant | operation |
|---|---|
| `ReluGrad_impl1` | fp16 `(feature > 0) ? gradient : 0`, one SIMD register width per invocation |
| `ReluGrad_impl2` | the same operation, eight fp16 (four `half2` packages) and one 16 byte vector load/store per invocation |
| `Relu_impl1` | `max(value, 0)` applied independently to the four signed bytes of every `int32` word, one word per thread |
| `Relu_impl2` | the same byte-wise maximum as a portable packed int8x4 SIMD operation, equivalent to `__vmaxs4` |

The fp16 variants evaluate the operation on binary16 bit patterns. `ReluGrad_impl2` uses an
`onAcc::SimdAlgo` concurrency budget of `8 * sizeof(uint16_t)` and handles the tail with the SIMD
algorithm's scalar remainder path. `Relu_impl2` runs `onAcc::SimdAlgo` over an `int8_t` byte span
with a four byte concurrency budget, so one invocation consumes exactly one `int32` word.

Data generation matches the HeCBench references: `std::mt19937` seeded with `19937`, the same
number and order of fp16 draws before the packed integers, and a fixed gradient of 1.

## Parameters

`--size <count>` and `--repeat <n>` select the number of elements and the measured iterations.
Defaults are `10000000` and `100`, the campaign values. The native HeCBench positional form
`<count> <repeat>` is accepted as well; named and positional arguments cannot be mixed. Every
variant is timed with the same arguments.

Timing follows the HeCBench scope. Each variant runs one discarded warm-up launch, then `repeat`
launches are enqueued back-to-back on a non-blocking queue under one timer and the queue is
synchronized once. The printed value is the average `total_us / repeat`. Allocation, host/device
copies and validation are outside the timed region. The output has one ORNL-compatible timing line
per variant:

```text
Average execution time of ReluGrad_impl1 Kernel: <value> (us)
Average execution time of ReluGrad_impl2 Kernel: <value> (us)
Average execution time of Relu_impl1 Kernel : <value> (us)
Average execution time of Relu_impl2 Kernel: <value> (us)
```

## What the test validates

`helper.hpp` contains an independent host reference. After each variant, its device output is
copied back and validated separately. ReluGrad is recomputed in double precision and compared with
the ORNL tolerance of `1e-3`; NaN and Inf results fail. The packed-int Relu output is compared
bit-exactly (byte-wise `memcmp`) against the per-byte reference. Each variant prints
`ReluGrad_impl1 PASS`, `ReluGrad_impl2 PASS`, `Relu_impl1 PASS` or `Relu_impl2 PASS`; any mismatch
makes the process exit non-zero.

The CTest entry runs `relu-alpaka 4096 2` and matches the four timing lines in stdout. A
validation failure or a missing line fails the test.

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
./build/cpu-serial/bin/alpaka/relu 4096 2
```

The named form is equivalent:

```bash
./build/cpu-serial/bin/alpaka/relu --size 4096 --repeat 2
```

CUDA, default `sm_80`:

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target relu-alpaka -j
./build/cuda/bin/alpaka/relu 4096 2
```

## Test

```bash
ctest --test-dir build/cpu-serial -R relu-alpaka --output-on-failure
```

The vendored metadata in `src/scripts/benchmarks/subset.json` uses the campaign arguments
`10000000 100`, so the upstream tooling can run the port:

```bash
python3 tools/hecbench -v run relu --model alpaka
```
