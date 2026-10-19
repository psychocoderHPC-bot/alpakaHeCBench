/* Copyright 2026 Rene Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

/** @file Portable, single-source alpaka implementation of the HeCBench `relu` benchmark.
 *
 * Four variants are covered, matching the HeCBench CUDA and SYCL reference programs:
 *  - ReluGrad_impl1: fp16 `(feature > 0) ? gradient : 0`, one SIMD register width per invocation.
 *  - ReluGrad_impl2: fp16 `(feature > 0) ? gradient : 0`, eight fp16 (four half2) per invocation.
 *  - Relu_impl1:     `max(value, 0)` applied independently to the four signed bytes of every int32 word.
 *  - Relu_impl2:     the same byte-wise maximum expressed as a portable packed int8x4 SIMD operation.
 *
 * The exact same kernel source is compiled for the host CPU and for CUDA GPUs. There is no backend branch and no
 * native CUDA/HIP/OpenMP kernel.
 */

#include <alpaka/alpaka.hpp>

#include "helper.hpp"
#include "misc.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <random>

namespace reluAlpaka
{
    /** Packed-integer ReLU: apply `max(byte, 0)` to each of the four signed bytes of an int32 word.
     *
     * This is the Relu_impl1 path of the HeCBench reference: one int32 word, four scalar byte maxima, per thread.
     */
    struct ReluIntKernel
    {
        ALPAKA_FN_ACC void operator()(
            auto const& acc,
            alpaka::concepts::IMdSpan auto output,
            alpaka::concepts::IMdSpan auto input,
            int count) const
        {
            using namespace alpaka;
            for(auto i : onAcc::makeIdxMap(acc, onAcc::worker::linearThreadsInGrid, IdxRange{count}))
            {
                int32_t const value = input[i];
                auto const c0 = static_cast<int8_t>(value & 0xFF);
                auto const c1 = static_cast<int8_t>((value >> 8) & 0xFF);
                auto const c2 = static_cast<int8_t>((value >> 16) & 0xFF);
                auto const c3 = static_cast<int8_t>((value >> 24) & 0xFF);
                uint32_t const r0 = static_cast<uint32_t>(c0 > 0 ? c0 : 0);
                uint32_t const r1 = static_cast<uint32_t>(c1 > 0 ? c1 : 0);
                uint32_t const r2 = static_cast<uint32_t>(c2 > 0 ? c2 : 0);
                uint32_t const r3 = static_cast<uint32_t>(c3 > 0 ? c3 : 0);
                output[i] = static_cast<int32_t>((r3 << 24) | (r2 << 16) | (r1 << 8) | r0);
            }
        }
    };

    /** Packed int8x4 ReLU: lane-wise `max(byte, 0)` over the bytes of an int32 word.
     *
     * This is the portable equivalent of `__vmaxs4(input, 0)`: four signed bytes are loaded as one int32 word,
     * processed as four SIMD lanes and stored back as one int32 word. Input and output spans are byte views.
     */
    struct ReluByteMaxOp
    {
        constexpr void operator()(auto const&, auto&& out, auto const& input) const
        {
            auto const value = input.load();
            using SimdType = ALPAKA_TYPEOF(value);
            constexpr uint32_t width = SimdType::width();
            SimdType result = SimdType::fill(int8_t{0});
            for(uint32_t i = 0u; i < width; ++i)
                result[i] = value[i] > int8_t{0} ? value[i] : int8_t{0};
            out = result;
        }
    };

    /** Relu_impl2: packed int8x4 maximum with a four byte SIMD concurrency budget.
     *
     * The input is addressed as a byte span. A concurrency budget of four bytes makes one SimdAlgo invocation
     * consume exactly one int32 word, i.e. four signed bytes, matching the reference's `__vmaxs4` operation.
     */
    struct ReluByteKernel
    {
        ALPAKA_FN_ACC void operator()(
            auto const& acc,
            alpaka::concepts::IMdSpan auto output,
            alpaka::concepts::IMdSpan auto input) const
        {
            using namespace alpaka;
            constexpr uint32_t simdWidthInByte = 4u * sizeof(int8_t);
            auto simdGrid = onAcc::SimdAlgo{onAcc::worker::threadsInGrid};
            simdGrid.template concurrent<simdWidthInByte>(acc, input.getExtents(), ReluByteMaxOp{}, output, input);
        }
    };

    /** Per-SIMD-package fp16 ReluGrad evaluated on the fp16 bit patterns.
     *
     * `(feature > 0) ? gradient : 0` depends only on the sign, magnitude and NaN-ness of the binary16 pattern, so it
     * is evaluated on the raw 16 bit lanes instead of converting to float and back. The branchless predicate
     * `(uint16_t)(bits - 1) < 0x7C00` is true exactly when the binary16 value is strictly positive:
     *   - `0x0000` and `0x8000` (both zeros) wrap to `0xFFFF`/`0x7FFF` and are rejected,
     *   - every negative pattern is `>= 0x8000` and rejected,
     *   - positive NaN payloads are `>= 0x7C00` after the decrement and rejected,
     *   - positive normals, positive subnormals and `+inf` (`0x7C00`) are accepted.
     * The remaining result is the gradient gated by the predicate `gradient & (predicate ? 0xFFFF : 0)`.
     */
    struct ReluGradOp
    {
        constexpr void operator()(auto const&, auto&& out, auto const& feature, auto const& gradient) const
        {
            auto const f = feature.load();
            auto const g = gradient.load();
            using SimdType = ALPAKA_TYPEOF(f);
            constexpr uint32_t width = SimdType::width();
            SimdType result = SimdType::fill(uint16_t{0});
            for(uint32_t i = 0u; i < width; ++i)
            {
                // (uint16_t)(bits - 1) < 0x7C00  <=>  binary16 value > 0 (see struct documentation)
                auto const bits = static_cast<uint16_t>(f[i]);
                bool const positive = static_cast<uint16_t>(bits - 1u) < 0x7C00u;
                result[i] = positive ? static_cast<uint16_t>(g[i]) : uint16_t{0};
            }
            out = result;
        }
    };

    /** ReluGrad_impl1: fp16 ReluGrad with one hardware SIMD register width per invocation.
     *
     * The operation is evaluated with the alpaka SIMD facilities so that wide vector loads/stores are issued on every
     * backend and the per-element software float conversion of the scalar implementation is avoided. This is the
     * half2/SIMD-width path of the HeCBench reference; additional instruction-level parallelism is exposed by
     * running several such invocations per thread.
     */
    struct ReluGradKernel
    {
        ALPAKA_FN_ACC void operator()(
            auto const& acc,
            alpaka::concepts::IMdSpan auto backprop,
            alpaka::concepts::IMdSpan auto feature,
            alpaka::concepts::IMdSpan auto gradient) const
        {
            using namespace alpaka;
            // Concurrency budget of exactly one SIMD register width in bytes for the fp16 value type. This keeps one
            // vector load in flight per invocation and avoids the extra register pressure of multiple simultaneous
            // packages.
            constexpr uint32_t simdWidthInByte
                = getArchSimdWidth<uint16_t>(ALPAKA_TYPEOF(acc.getApi()){}, ALPAKA_TYPEOF(acc.getDeviceKind()){})
                  * sizeof(uint16_t);
            auto simdGrid = onAcc::SimdAlgo{onAcc::worker::threadsInGrid};
            simdGrid.template concurrent<simdWidthInByte>(
                acc,
                feature.getExtents(),
                ReluGradOp{},
                backprop,
                feature,
                gradient);
        }
    };

    /** ReluGrad_impl2: eight fp16 (four half2 packages) per thread and invocation.
     *
     * A 16 byte concurrency budget makes the SIMD algorithm consume eight fp16 values per invocation, matching the
     * reference's four `half2` operations behind one 16 byte vector load and store per array. The tail elements are
     * handled by the SIMD algorithm's scalar remainder path.
     */
    struct ReluGradVectorKernel
    {
        ALPAKA_FN_ACC void operator()(
            auto const& acc,
            alpaka::concepts::IMdSpan auto backprop,
            alpaka::concepts::IMdSpan auto feature,
            alpaka::concepts::IMdSpan auto gradient) const
        {
            using namespace alpaka;
            constexpr uint32_t simdWidthInByte = 8u * sizeof(uint16_t);
            auto simdGrid = onAcc::SimdAlgo{onAcc::worker::threadsInGrid};
            simdGrid.template concurrent<simdWidthInByte>(
                acc,
                feature.getExtents(),
                ReluGradOp{},
                backprop,
                feature,
                gradient);
        }
    };

    /** Build a frame specification that gives every worker a useful amount of work.
     *
     * @param elementsPerWorker elements each worker should process. Pass 0 to use the device minimum. The value must
     *        be a multiple of the elements consumed by a single `onAcc::SimdAlgo::concurrent()` invocation, otherwise
     *        the SIMD loop is skipped and all work falls into the scalar remainder loop.
     */
    template<typename T_Data>
    auto makeFrameSpec(auto const& device, auto const& exec, uint32_t extent, uint32_t elementsPerWorker)
    {
        using namespace alpaka;
        if(elementsPerWorker == 0u)
            elementsPerWorker = getNumElemPerThread<T_Data>(device);
        constexpr uint32_t chunkSize = 256u;
        uint32_t const blocks = alpaka::divCeil(std::max(extent, 1u), chunkSize * elementsPerWorker);
        return onHost::FrameSpec{std::max(blocks, 1u), chunkSize, exec};
    }

    /** Run `repeat` measured iterations of a kernel after one discarded warm-up iteration.
     *
     * All launches of the measured region are enqueued back-to-back on a non-blocking queue and the queue is waited
     * for once, matching the HeCBench timing scope. The reported value is the average total time per launch in
     * microseconds.
     */
    template<typename T_KernelBundle>
    double timeKernel(auto& queue, auto const& frameSpec, T_KernelBundle const& kernel, int repeat)
    {
        using namespace alpaka;
        // discarded warm-up
        queue.enqueue(frameSpec, kernel);
        onHost::wait(queue);

        auto const start = std::chrono::steady_clock::now();
        for(int i = 0; i < repeat; ++i)
            queue.enqueue(frameSpec, kernel);
        onHost::wait(queue);
        auto const end = std::chrono::steady_clock::now();

        double const totalUs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() * 1e-3;
        return totalUs / static_cast<double>(repeat);
    }

    auto example(auto const deviceSpec, auto const exec, int count, int repeat) -> int
    {
        using namespace alpaka;

        std::cout << "Problem size (elements): " << count << std::endl;
        std::cout << "Measured iterations: " << repeat << std::endl;
        std::cout << "Using alpaka accelerator " << onHost::demangledName(exec) << " for "
                  << deviceSpec.getApi().getName() << " " << deviceSpec.getDeviceKind().getName() << std::endl;

        auto devSelector = onHost::makeDeviceSelector(deviceSpec);
        onHost::Device device = devSelector.makeDevice(0);
        // A non-blocking queue keeps the timed region to back-to-back enqueues plus one wait, so GPU and host devices
        // measure the same launch-to-wait scope as the HeCBench reference.
        onHost::Queue queue = device.makeQueue(queueKind::nonBlocking);

        int result = EXIT_SUCCESS;

        // ---------------------------------------------------------------------------------------------
        // ReluGrad: fp16, impl1 and impl2
        // ---------------------------------------------------------------------------------------------
        {
            Vec<int, 1u> const extent(count);
            auto hFeature = onHost::allocHost<uint16_t>(extent);
            auto hGradient = onHost::allocHost<uint16_t>(extent);
            auto hBackprop = onHost::allocHost<uint16_t>(extent);

            std::mt19937 engine(19937);
            std::uniform_real_distribution<float> realDist(-1.f, 1.f);
            for(int i = 0; i < count; ++i)
            {
                hFeature[i] = relu::floatToHalf(realDist(engine));
                hGradient[i] = relu::floatToHalf(1.0f);
            }

            auto dFeature = onHost::alloc<uint16_t>(device, extent);
            auto dGradient = onHost::alloc<uint16_t>(device, extent);
            auto dBackprop = onHost::alloc<uint16_t>(device, extent);
            onHost::memcpy(queue, dFeature, hFeature);
            onHost::memcpy(queue, dGradient, hGradient);

            // impl1: one SIMD register width per invocation, several invocations per thread.
            {
                uint32_t const elementsPerWorker = 4u * getNumElemPerThread<uint16_t>(device);
                auto const frameSpec
                    = makeFrameSpec<uint16_t>(device, exec, static_cast<uint32_t>(count), elementsPerWorker);
                KernelBundle const kernel{ReluGradKernel{}, dBackprop, dFeature, dGradient};
                printf(
                    "Average execution time of ReluGrad_impl1 Kernel: %f (us)\n",
                    timeKernel(queue, frameSpec, kernel, repeat));

                onHost::memcpy(queue, hBackprop, dBackprop);
                onHost::wait(queue);
                int const check = relu::verifyReluGrad(count, hBackprop, hFeature, hGradient);
                printf("ReluGrad_impl1 %s\n", check == EXIT_SUCCESS ? "PASS" : "FAIL");
                result |= check;
            }

            // impl2: eight fp16 (four half2 packages) per thread and invocation.
            {
                constexpr uint32_t elementsPerWorker = 8u;
                auto const frameSpec
                    = makeFrameSpec<uint16_t>(device, exec, static_cast<uint32_t>(count), elementsPerWorker);
                KernelBundle const kernel{ReluGradVectorKernel{}, dBackprop, dFeature, dGradient};
                printf(
                    "Average execution time of ReluGrad_impl2 Kernel: %f (us)\n",
                    timeKernel(queue, frameSpec, kernel, repeat));

                onHost::memcpy(queue, hBackprop, dBackprop);
                onHost::wait(queue);
                int const check = relu::verifyReluGrad(count, hBackprop, hFeature, hGradient);
                printf("ReluGrad_impl2 %s\n", check == EXIT_SUCCESS ? "PASS" : "FAIL");
                result |= check;
            }
        }

        // ---------------------------------------------------------------------------------------------
        // Relu: packed int8x4, impl1 and impl2
        // ---------------------------------------------------------------------------------------------
        {
            Vec<int, 1u> const extent(count);
            Vec<int, 1u> const byteExtent(count * static_cast<int>(sizeof(int32_t)));
            auto hIn = onHost::allocHost<int32_t>(extent);
            auto hInBytes = onHost::allocHost<int8_t>(byteExtent);
            auto hOutBytes = onHost::allocHost<int8_t>(byteExtent);
            auto hRef = onHost::allocHost<int32_t>(extent);

            std::mt19937 engine(19937);
            std::uniform_real_distribution<float> realDist(-1.f, 1.f);
            // The HeCBench reference consumes the fp16 random numbers before generating the integers. Reproduce the
            // same engine state so that the integer inputs match the reference exactly.
            for(int i = 0; i < count; ++i)
                (void) realDist(engine);
            std::uniform_int_distribution<unsigned char> intDist(0, 255);
            for(int i = 0; i < count; ++i)
            {
                hIn[i] = static_cast<int32_t>(
                    static_cast<unsigned>(intDist(engine)) | (static_cast<unsigned>(intDist(engine)) << 8)
                    | (static_cast<unsigned>(intDist(engine)) << 16)
                    | (static_cast<unsigned>(intDist(engine)) << 24));
                hRef[i] = relu::reluReference(hIn[i]);
            }
            std::memcpy(hInBytes.data(), hIn.data(), static_cast<std::size_t>(count) * sizeof(int32_t));

            // impl1: one int32 word per thread with four scalar byte maxima.
            {
                auto dIn = onHost::alloc<int32_t>(device, extent);
                auto dOut = onHost::alloc<int32_t>(device, extent);
                onHost::memcpy(queue, dIn, hIn);

                auto const frameSpec = makeFrameSpec<int32_t>(device, exec, static_cast<uint32_t>(count), 0u);
                KernelBundle const kernel{ReluIntKernel{}, dOut, dIn, count};
                printf(
                    "Average execution time of Relu_impl1 Kernel : %f (us)\n",
                    timeKernel(queue, frameSpec, kernel, repeat));

                onHost::memcpy(queue, hOutBytes, dOut);
                onHost::wait(queue);
                int const check
                    = std::memcmp(hOutBytes.data(), hRef.data(), static_cast<std::size_t>(count) * sizeof(int32_t))
                          == 0
                          ? EXIT_SUCCESS
                          : EXIT_FAILURE;
                printf("Relu_impl1 %s\n", check == EXIT_SUCCESS ? "PASS" : "FAIL");
                result |= check;
            }

            // impl2: one int32 word per invocation as four packed int8 lanes.
            {
                auto dInBytes = onHost::alloc<int8_t>(device, byteExtent);
                auto dOutBytes = onHost::alloc<int8_t>(device, byteExtent);
                onHost::memcpy(queue, dInBytes, hInBytes);

                constexpr uint32_t elementsPerWorker = 4u;
                auto const frameSpec
                    = makeFrameSpec<int8_t>(device, exec, static_cast<uint32_t>(count * 4), elementsPerWorker);
                KernelBundle const kernel{ReluByteKernel{}, dOutBytes, dInBytes};
                printf(
                    "Average execution time of Relu_impl2 Kernel: %f (us)\n",
                    timeKernel(queue, frameSpec, kernel, repeat));

                onHost::memcpy(queue, hOutBytes, dOutBytes);
                onHost::wait(queue);
                int const check
                    = std::memcmp(hOutBytes.data(), hRef.data(), static_cast<std::size_t>(count) * sizeof(int32_t))
                          == 0
                          ? EXIT_SUCCESS
                          : EXIT_FAILURE;
                printf("Relu_impl2 %s\n", check == EXIT_SUCCESS ? "PASS" : "FAIL");
                result |= check;
            }
        }

        return result;
    }
} // namespace reluAlpaka

auto main(int argc, char* argv[]) -> int
{
    int count = 10000000;
    int repeat = 100;

    if(int const ret = relu::parseCmd(argc, argv, count, repeat))
        return ret;

    using namespace alpaka;

    return onHost::executeForEachIfHasDevice(
        [=](auto const& backend)
        { return reluAlpaka::example(onHost::makeDeviceSpec(backend), getExecutor(backend), count, repeat); },
        onHost::allBackends(onHost::enabledDeviceSpecs, exec::enabledExecutors));
}
