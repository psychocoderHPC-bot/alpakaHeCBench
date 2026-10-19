/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

/** @file Portable, single-source alpaka implementation of the HeCBench `mandelbrot` benchmark.
 *
 * This translation unit is original work. It includes `helper.hpp`, which contains HeCBench code
 * under the MIT license, Copyright (c) 2019 Intel Corporation; the third-party license texts and
 * the full provenance are reproduced in `THIRD-PARTY-NOTICES.txt`.
 *
 * The HeCBench references (`src/mandelbrot-cuda`, `src/mandelbrot-omp`) evaluate the escape-time
 * iteration of the Mandelbrot set over a fixed image. `MandelParameters::Point` is the exact same
 * scalar iteration in every reference; only the loop that walks the image differs (a CUDA kernel
 * with a 16x16 thread block or an OpenMP `target teams distribute parallel for collapse(2)`).
 *
 * This port follows the protocol: one portable kernel source, compiled unchanged for the host CPU
 * and for CUDA GPUs. There is no backend branch and no native CUDA/HIP/OpenMP kernel. The sampling
 * grid and the iteration body are shared with the host reference in `helper.hpp`, so the reference
 * and the kernel are arithmetically identical.
 */

#include <alpaka/alpaka.hpp>

// alpaka's SYCL backend defines a global printf macro for device printf;
// the host reporting here uses std::printf. Undefine it so <cstdio> names
// resolve to the host functions.
#undef printf

#include "helper.hpp"
#include "misc.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <tuple>
#include <type_traits>

namespace mandelbrotAlpaka
{
    /** Escape-time iteration for every pixel of the image.
     *
     * The image is walked as a 2D range (the reference walks the same 2D image with
     * `collapse(2)`); the per-pixel work is the shared `MandelParameters::Point`.
     * `MandelParameters` is passed by value into the kernel, exactly like the reference copies it to
     * the device.
     */
    struct MandelKernel
    {
        ALPAKA_FN_ACC void operator()(
            auto const& acc,
            alpaka::concepts::IMdSpan auto output,
            mandel::MandelParameters params,
            int rows,
            int cols) const
        {
            using namespace alpaka;
            for(auto [j, i] :
                onAcc::makeIdxMap(acc, onAcc::worker::threadsInGrid, IdxRange{Vec{cols, rows}}))
            {
                output[i * cols + j] = static_cast<float>(
                    params.point(mandel::ComplexF{params.scaleRow(i), params.scaleCol(j)}));
            }
        }
    };

    /** Build a 2D frame specification with one pixel per worker, 16 x 16 per frame.
     *
     * The dimension order follows the index range: dimension 0 is the column and dimension 1 is the
     * row.
     */
    auto makeFrameSpec(auto const&, auto const& exec, uint32_t rows, uint32_t cols)
    {
        using namespace alpaka;
        constexpr uint32_t chunk = 16u;
        uint32_t const blocksCols = divCeil(std::max(cols, 1u), chunk);
        uint32_t const blocksRows = divCeil(std::max(rows, 1u), chunk);
        return onHost::FrameSpec{Vec{std::max(blocksCols, 1u), std::max(blocksRows, 1u)}, Vec{chunk, chunk}, exec};
    }

    /** Sentinel returned by runFirstAvailable() when no backend matched. */
    inline constexpr int backendSkipped = -1;

    /** Run the first available backend that matches `predicate`.
     *
     * The candidate list is a tuple of heterogeneous backend dictionaries, so
     * selection is a compile-time expansion with a runtime availability check.
     *
     * @return the backend's exit code, or backendSkipped when none matched.
     */
    template<typename T_Backends, typename T_Predicate, typename T_Run>
    int runFirstAvailable(T_Backends const& backends, T_Predicate const& predicate, T_Run const& run)
    {
        int result = backendSkipped;
        bool selected = false;
        std::apply(
            [&](auto const&... backend)
            {
                auto const tryBackend = [&](auto const& candidate)
                {
                    if(selected)
                        return;
                    if(!predicate(candidate))
                        return;
                    auto const deviceSpec = alpaka::onHost::makeDeviceSpec(candidate);
                    if(!alpaka::onHost::makeDeviceSelector(deviceSpec).isAvailable())
                        return;
                    selected = true;
                    result = run(candidate);
                };
                (tryBackend(backend), ...);
            },
            backends);
        return selected ? result : backendSkipped;
    }

    /** Time `repeat` kernel launches under one timer with a single wait.
     *
     * This mirrors the HeCBench reference: there is no warm-up, and the reported
     * value is the arithmetic mean over the measured launches.
     *
     * @return the arithmetic mean runtime in milliseconds.
     */
    template<typename T_KernelBundle>
    double timeKernel(auto& queue, auto const& frameSpec, T_KernelBundle const& kernel, int repeat)
    {
        using namespace alpaka;
        auto const start = std::chrono::steady_clock::now();
        for(int i = 0; i < repeat; ++i)
            queue.enqueue(frameSpec, kernel);
        onHost::wait(queue);
        auto const end = std::chrono::steady_clock::now();
        auto const totalMs = std::chrono::duration<double, std::milli>(end - start).count();
        return totalMs / static_cast<double>(repeat);
    }

    auto example(auto const deviceSpec, auto const exec, int rows, int cols, int maxIterations, int repeat) -> int
    {
        using namespace alpaka;

        long long const total = static_cast<long long>(rows) * static_cast<long long>(cols);
        std::cout << "Image size: " << rows << " x " << cols << " (" << total << " pixels)" << std::endl;
        std::cout << "Max iterations: " << maxIterations << std::endl;
        std::cout << "Measured iterations: " << repeat << std::endl;
        std::cout << "Using alpaka accelerator " << onHost::demangledName(exec) << " for "
                  << deviceSpec.getApi().getName() << " " << deviceSpec.getDeviceKind().getName() << std::endl;

        auto devSelector = onHost::makeDeviceSelector(deviceSpec);
        onHost::Device device = devSelector.makeDevice(0);
        onHost::Queue queue = device.makeQueue(queueKind::blocking);

        Vec<int, 1u> const extent(static_cast<int>(total));
        auto hOut = onHost::allocHost<float>(extent);
        auto hReferenceFloat = onHost::allocHost<float>(extent);
        auto hReferenceDouble = onHost::allocHost<double>(extent);

        // Host references: the faithful float iteration and an independent double iteration.
        mandel::evaluateReferenceFloat(rows, cols, maxIterations, hReferenceFloat.data());
        mandel::evaluateReferenceDouble(rows, cols, maxIterations, hReferenceDouble.data());

        auto dOut = onHost::alloc<float>(device, extent);
        // Poison the device image with a NaN bit pattern so a pixel the kernel does not write is
        // caught by the finite check in verify() instead of silently matching the reference.
        onHost::memset(queue, dOut, 0xFFu);
        onHost::wait(queue);

        mandel::MandelParameters const params{rows, cols, maxIterations};
        auto const frameSpec = makeFrameSpec(device, exec, static_cast<uint32_t>(rows), static_cast<uint32_t>(cols));
        KernelBundle const kernel{MandelKernel{}, dOut, params, rows, cols};

        double const meanMs = timeKernel(queue, frameSpec, kernel, repeat);
        // The upstream HeCBench reference logs the arithmetic mean kernel time in
        // milliseconds and the vendored test metadata matches that field.
        std::printf("Average kernel execution time: %.6f ms\n", meanMs);

        onHost::memcpy(queue, hOut, dOut);
        onHost::wait(queue);

        return mandel::verify(hOut, rows, cols, maxIterations, hReferenceFloat.data(), hReferenceDouble.data());
    }
} // namespace mandelbrotAlpaka

auto main(int argc, char* argv[]) -> int
{
    int rows = mandel::defaultRowSize;
    int cols = mandel::defaultColSize;
    int maxIterations = mandel::defaultMaxIterations;
    int repeat = mandel::defaultRepetitions;
    bool allBackends = false;

    if(int const ret = mandel::parseCmd(argc, argv, rows, cols, maxIterations, repeat, allBackends))
        return ret;

    using namespace alpaka;

    auto const backends = onHost::allBackends(onHost::enabledDeviceSpecs, exec::enabledExecutors);
    auto runBackend = [=](auto const& backend)
    {
        return mandelbrotAlpaka::example(
            onHost::makeDeviceSpec(backend),
            getExecutor(backend),
            rows,
            cols,
            maxIterations,
            repeat);
    };

    if(allBackends)
    {
        return onHost::executeForEachIfHasDevice(runBackend, backends);
    }

    // The harness matches every "Average kernel execution time" line and sums
    // them, so the default run must use exactly one backend/executor. Prefer a
    // non-host GPU, then the host OpenMP executor, then the host serial
    // executor; fall back to the first available backend.
    auto const isGpu = [](auto const& backend)
    {
        using DeviceKind = ALPAKA_TYPEOF(getDeviceKind(backend));
        return !std::is_same_v<DeviceKind, deviceKind::Cpu>
            && !std::is_same_v<DeviceKind, deviceKind::NumaCpu>;
    };
    auto const isHostOmp = [](auto const& backend)
    {
        return std::is_same_v<ALPAKA_TYPEOF(getApi(backend)), api::Host>
            && std::is_same_v<ALPAKA_TYPEOF(getExecutor(backend)), exec::CpuOmpBlocks>;
    };
    auto const isHostSerial = [](auto const& backend)
    {
        return std::is_same_v<ALPAKA_TYPEOF(getApi(backend)), api::Host>
            && std::is_same_v<ALPAKA_TYPEOF(getExecutor(backend)), exec::CpuSerial>;
    };
    auto const anyBackend = [](auto const&) { return true; };

    constexpr int skipped = mandelbrotAlpaka::backendSkipped;
    if(int const ret = mandelbrotAlpaka::runFirstAvailable(backends, isGpu, runBackend); ret != skipped)
        return ret;
    if(int const ret = mandelbrotAlpaka::runFirstAvailable(backends, isHostOmp, runBackend); ret != skipped)
        return ret;
    if(int const ret = mandelbrotAlpaka::runFirstAvailable(backends, isHostSerial, runBackend); ret != skipped)
        return ret;
    if(int const ret = mandelbrotAlpaka::runFirstAvailable(backends, anyBackend, runBackend); ret != skipped)
        return ret;

    std::cerr << "Error: no supported backend is available.\n";
    return EXIT_FAILURE;
}
