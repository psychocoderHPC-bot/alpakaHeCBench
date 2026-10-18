<!--
SPDX-FileCopyrightText: 2026 René Widera

SPDX-License-Identifier: CC0-1.0
-->

# black-scholes (alpaka)

Portable single-source [alpaka](https://github.com/alpaka-group/alpaka) port of
the HeCBench `black-scholes` benchmark. One C++20 translation unit prices a long
stream of European call and put options and is compiled unchanged for the host
CPU and for a CUDA GPU. There is no backend branch and no native CUDA/HIP/OpenMP
kernel.

## What it computes

The input stream contains `--options` entries; entry `i` is one of the 37
QuantLib test settings from the reference, selected by `i % 37`. Each setting is
a plain-vanilla `CALL` or `PUT` with strike, spot, dividend yield `q`, risk-free
rate `r`, time to maturity `t` and Black volatility `vol`.

Every option goes through the reference's Black-Scholes analytic engine pipeline
(ported from QuantLib):

- dividend discount `1/exp(q*t)` and risk-free discount `1/exp(r*t)`;
- forward `spot*dividendDiscount/riskFreeDiscount`;
- standard deviation `sqrt(vol^2*t)`;
- `d1 = log(forward/strike)/stdDev + stdDev/2`, `d2 = d1 - stdDev`;
- the normal CDF from the reference's single-precision Cody-Waite `erf`
  approximation (`errorFunct`) with the constants from `errorFunctConsts.cuh`;
- price `riskFreeDiscount * (forward*alpha + strike*beta)` with the call/put
  `alpha`/`beta` conventions of `initBlackCalcVars`.

The port keeps the reference algorithm and single-precision arithmetic. One
work item owns one option, so the same source is correct on the host CPU and on
NVIDIA GPUs.

## Parameters

- Positional HeCBench form: `black-scholes <repeat>`. The reference `run` target
  invokes `./main 100`, and the upstream `subset.json` argument list is `["100"]`.
- Named form: `black-scholes --options N --repeat R --samples S [--verify-only]`.
- `--options` defaults to `50000000`, matching the reference `main`.
- `--repeat` defaults to `100`: kernel launches per measured sample.
- `--samples` defaults to `5`: independent measured samples.
- `--verify-only` runs the correctness check without the timing loop.
- `-h` / `--help` prints the usage.

The positional repeat and `--repeat` are both accepted, but not at the same
time. The input stream is generated internally; no input file is read.

## Test semantics

`verify()` compares every device price against an independent double-precision
host oracle (`referencePrice` in `helper.hpp`), which evaluates the same
Black-Scholes formulas with `std::erfc` and shares no code with the port. It
rejects any NaN or Inf, reports the maximum absolute deviation and the first
offending index, and exits non-zero on failure. The acceptance tolerance is the
reference's own absolute tolerance `1.0e-4`; it is not widened.

As a secondary, informational check `verify()` also prints the maximum deviation
from the published analytic prices of the 37 settings. That value does not
change pass/fail.

The measured metric is the median of `--samples` independent samples after one
warm-up, where each sample runs `--repeat` kernel launches end to end and
reports `total / repeat`. Validation runs a separate single launch after the
timing loop.

## Provenance

- Reference: [ORNL/HeCBench](https://github.com/ORNL/HeCBench) at commit
  [`0a17ee47ca6072192101036d4bf292e6612f22e6`](https://github.com/ORNL/HeCBench/commit/0a17ee47ca6072192101036d4bf292e6612f22e6).
  - CUDA harness: `src/black-scholes-cuda/blackScholesAnalyticEngine.cu`
  - CUDA kernels: `src/black-scholes-cuda/blackScholesAnalyticEngineKernels.cu`,
    `src/black-scholes-cuda/blackScholesAnalyticEngineKernels.cuh`
  - CUDA CPU kernels: `src/black-scholes-cuda/blackScholesAnalyticEngineKernelsCpu.cu`,
    `src/black-scholes-cuda/blackScholesAnalyticEngineKernelsCpu.cuh`
  - Structs and constants: `src/black-scholes-cuda/blackScholesAnalyticEngineStructs.cuh`,
    `src/black-scholes-cuda/errorFunctConsts.cuh`
  - OpenMP harness: `src/black-scholes-omp/blackScholesAnalyticEngine.cpp`
- Source port: `psychocoderHPC-bot/alpaka3` PR #53, head
  `8903b8dc096c36de5428a7e5d60e0c6521614528`, `benchmark/black-scholes`.
- The upstream reference sources are kept outside this repository and are not
  vendored. This clean-room alpaka reimplementation is committed under
  `BSD-3-Clause`; `misc.hpp` contains no parts of the HeCBench code base.
- The analytic engine is ported from QuantLib; the source headers credit Scott
  Grauer-Gray. The HeCBench repository is BSD 3-Clause.

## Build and run

Build from the repository root with the CPU serial preset:

```bash
cmake --preset cpu-serial -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cpu-serial --target black-scholes-alpaka -j
```

Run a short explicit configuration:

```bash
./build/cpu-serial/bin/alpaka/black-scholes --options 37000 --repeat 100 --samples 2
```

Run the upstream positional form:

```bash
./build/cpu-serial/bin/alpaka/black-scholes 100
```

The same target builds with the `cuda` preset. The preset defaults to `sm_80`;
pass `-DCMAKE_CUDA_ARCHITECTURES=<n>` to target another GPU.

```bash
cmake --preset cuda -Dalpaka_COMPILE_PEDANTIC=ON .
cmake --build build/cuda --target black-scholes-alpaka -j
./build/cuda/bin/alpaka/black-scholes --options 50000000 --repeat 100 --samples 5
```

The benchmark prints a `RESULT` line with the median, minimum, maximum and
relative spread of the samples, the upstream-compatible
`Processing time on GPU: <value> (ms)` line and a final
`black-scholes options=<n> verification PASSED` line.

## CTest

The CMake test registration runs the upstream positional repeat with a small
option stream so the CPU serial test stays fast:

```bash
ctest --test-dir build/cpu-serial -R black-scholes-alpaka --output-on-failure
```
