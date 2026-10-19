/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

/** @file Portable, single-source alpaka implementation of the HeCBench `popcount` benchmark.
 *
 * The HeCBench reference program evaluates the same operation with six algorithms: three SWAR
 * variants, Kernighan's `x &= x - 1` loop, a fixed 64 iteration bit loop, a 256 entry lookup table
 * and the hardware popcount intrinsic. This port implements all six variants as alpaka kernels so
 * that every timed variant is a real device kernel. The exact same kernel source is compiled for
 * the host CPU and for CUDA GPUs; there is no backend branch and no native CUDA/HIP/OpenMP kernel.
 */

#include <alpaka/alpaka.hpp>

#include "helper.hpp"
#include "misc.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>

namespace popcountAlpaka
{
    /** pc1: SWAR with masks, horizontal byte adds and a final 7 bit mask. */
    struct Pc1Swar
    {
        ALPAKA_FN_ACC void operator()(
            auto const& acc,
            alpaka::concepts::IMdSpan auto output,
            alpaka::concepts::IMdSpan auto input,
            int count) const
        {
            using namespace alpaka;
            constexpr uint64_t m1 = 0x5555555555555555ull;
            constexpr uint64_t m2 = 0x3333333333333333ull;
            constexpr uint64_t m4 = 0x0f0f0f0f0f0f0f0full;
            for(auto i : onAcc::makeIdxMap(acc, onAcc::worker::linearThreadsInGrid, IdxRange{count}))
            {
                uint64_t x = static_cast<uint64_t>(input[i]);
                x -= (x >> 1) & m1;
                x = (x & m2) + ((x >> 2) & m2);
                x = (x + (x >> 4)) & m4;
                x += x >> 8;
                x += x >> 16;
                x += x >> 32;
                output[i] = static_cast<int32_t>(x & 0x7f);
            }
        }
    };

    /** pc2: SWAR through the nibble fold, then multiply and keep the top byte. */
    struct Pc2SwarMultiply
    {
        ALPAKA_FN_ACC void operator()(
            auto const& acc,
            alpaka::concepts::IMdSpan auto output,
            alpaka::concepts::IMdSpan auto input,
            int count) const
        {
            using namespace alpaka;
            constexpr uint64_t m1 = 0x5555555555555555ull;
            constexpr uint64_t m2 = 0x3333333333333333ull;
            constexpr uint64_t m4 = 0x0f0f0f0f0f0f0f0full;
            constexpr uint64_t h01 = 0x0101010101010101ull;
            for(auto i : onAcc::makeIdxMap(acc, onAcc::worker::linearThreadsInGrid, IdxRange{count}))
            {
                uint64_t x = static_cast<uint64_t>(input[i]);
                x -= (x >> 1) & m1;
                x = (x & m2) + ((x >> 2) & m2);
                x = (x + (x >> 4)) & m4;
                output[i] = static_cast<int32_t>((x * h01) >> 56);
            }
        }
    };

    /** pc3: Kernighan's loop, clearing one set bit per iteration. */
    struct Pc3Kernighan
    {
        ALPAKA_FN_ACC void operator()(
            auto const& acc,
            alpaka::concepts::IMdSpan auto output,
            alpaka::concepts::IMdSpan auto input,
            int count) const
        {
            using namespace alpaka;
            for(auto idx : onAcc::makeIdxMap(acc, onAcc::worker::linearThreadsInGrid, IdxRange{count}))
            {
                uint64_t x = static_cast<uint64_t>(input[idx]);
                int32_t bits = 0;
                for(bits = 0; x; ++bits)
                    x &= x - 1;
                output[idx] = bits;
            }
        }
    };

    /** pc4: fixed 64 iteration bit loop. */
    struct Pc4BitLoop
    {
        ALPAKA_FN_ACC void operator()(
            auto const& acc,
            alpaka::concepts::IMdSpan auto output,
            alpaka::concepts::IMdSpan auto input,
            int count) const
        {
            using namespace alpaka;
            for(auto idx : onAcc::makeIdxMap(acc, onAcc::worker::linearThreadsInGrid, IdxRange{count}))
            {
                uint64_t x = static_cast<uint64_t>(input[idx]);
                int32_t bits = 0;
                for(int32_t bit = 0; bit < 64; ++bit)
                {
                    bits += static_cast<int32_t>(x & 0x1ull);
                    x >>= 1;
                }
                output[idx] = bits;
            }
        }
    };

    /** pc5: eight byte lookups in a 256 entry table, summed.
     *
     * The table lives in portable alpaka block shared memory. Every block fills the 256 entries
     * cooperatively, then synchronizes, so the lookups hit on-chip memory on CUDA and on every
     * host executor, matching the constant-memory table placement of the HeCBench reference.
     */
    struct Pc5LookupTable
    {
        ALPAKA_FN_ACC void operator()(
            auto const& acc,
            alpaka::concepts::IMdSpan auto output,
            alpaka::concepts::IMdSpan auto input,
            int count) const
        {
            using namespace alpaka;
            auto& lut = onAcc::declareSharedVar<unsigned char[256], uniqueId()>(acc);
            for(auto entry : onAcc::makeIdxMap(acc, onAcc::worker::threadsInBlock, IdxRange{256u}))
                lut[entry.x()] = static_cast<unsigned char>(alpaka::popcount(static_cast<uint32_t>(entry.x())));
            onAcc::syncBlockThreads(acc);
            for(auto idx : onAcc::makeIdxMap(acc, onAcc::worker::linearThreadsInGrid, IdxRange{count}))
            {
                uint64_t const x = static_cast<uint64_t>(input[idx]);
                uint32_t const i1 = lut[x & 0xffu];
                uint32_t const i2 = lut[(x >> 8) & 0xffu];
                uint32_t const i3 = lut[(x >> 16) & 0xffu];
                uint32_t const i4 = lut[(x >> 24) & 0xffu];
                uint32_t const i5 = lut[(x >> 32) & 0xffu];
                uint32_t const i6 = lut[(x >> 40) & 0xffu];
                uint32_t const i7 = lut[(x >> 48) & 0xffu];
                uint32_t const i8 = lut[(x >> 56) & 0xffu];
                output[idx] = static_cast<int32_t>((i1 + i2) + (i3 + i4) + (i5 + i6) + (i7 + i8));
            }
        }
    };

    /** pc6: portable hardware popcount intrinsic. */
    struct Pc6Intrinsic
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
                output[i] = static_cast<int32_t>(alpaka::popcount(static_cast<uint64_t>(input[i])));
            }
        }
    };

    /** Build a frame specification that gives every worker a useful amount of work. */
    template<typename T_Data>
    auto makeFrameSpec(auto const& device, auto const& exec, uint32_t extent)
    {
        using namespace alpaka;
        constexpr uint32_t chunkSize = 256u;
        uint32_t const elementsPerWorker = getNumElemPerThread<T_Data>(device);
        uint32_t const blocks = alpaka::divCeil(std::max(extent, 1u), chunkSize * elementsPerWorker);
        return onHost::FrameSpec{std::max(blocks, 1u), chunkSize, exec};
    }

    /** Reject host results outside the valid popcount range `[0, 64]`. */
    inline int checkRange(int count, auto const& out)
    {
        constexpr int maxPopcount = static_cast<int>(8u * sizeof(uint64_t));
        for(int i = 0; i < count; ++i)
        {
            int const value = static_cast<int>(out[i]);
            if(value < 0 || value > maxPopcount)
            {
                printf("range error at result[%d]: %d outside [0, %d]\n", i, value, maxPopcount);
                return EXIT_FAILURE;
            }
        }
        return EXIT_SUCCESS;
    }

    /** Time one discarded warm-up launch followed by `repeat` launches under one timer.
     *
     * The run queue is non-blocking, so the host does not synchronize inside every enqueue. All
     * `repeat` launches are queued back-to-back, then the queue is synchronized once. The reported
     * value is `total_us / repeat`, the same launch-to-wait scope as the HeCBench reference on GPU
     * and CPU.
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

    /** Run, time and validate one variant against the Kernighan reference. */
    template<typename T_Kernel>
    int runVariant(
        char const* name,
        auto& queue,
        auto const& frameSpec,
        auto& dIn,
        auto& dOut,
        auto const& hIn,
        auto& hOut,
        int count,
        int repeat)
    {
        using namespace alpaka;
        KernelBundle const kernel{T_Kernel{}, dOut, dIn, count};
        printf("Average kernel execution time (%s): %f (us)\n", name, timeKernel(queue, frameSpec, kernel, repeat));

        onHost::memcpy(queue, hOut, dOut);
        onHost::wait(queue);
        if(checkRange(count, hOut) != EXIT_SUCCESS)
        {
            printf("%s Fail\n", name);
            return EXIT_FAILURE;
        }
        return popcount::verify(name, count, hOut, hIn);
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
        // A non-blocking queue keeps the timed region to back-to-back enqueues plus one wait, so
        // GPU and host devices measure the same launch-to-wait scope as the HeCBench reference.
        onHost::Queue queue = device.makeQueue(queueKind::nonBlocking);

        Vec<int, 1u> const extent(count);
        auto hIn = onHost::allocHost<uint64_t>(extent);
        auto hOut = onHost::allocHost<int32_t>(extent);

        // Input generation matches the HeCBench reference: srand(2) and two rand() draws per element,
        // the first shifted into the high 32 bits.
        std::srand(2);
        for(int i = 0; i < count; ++i)
        {
            uint64_t const high = static_cast<uint64_t>(static_cast<uint32_t>(std::rand())) << 32;
            uint64_t const low = static_cast<uint32_t>(std::rand());
            hIn[i] = high | low;
        }

        auto dIn = onHost::alloc<uint64_t>(device, extent);
        auto dOut = onHost::alloc<int32_t>(device, extent);
        onHost::memcpy(queue, dIn, hIn);

        auto const frameSpec = makeFrameSpec<uint64_t>(device, exec, static_cast<uint32_t>(count));

        int error = EXIT_SUCCESS;
        error |= runVariant<Pc1Swar>("pc1", queue, frameSpec, dIn, dOut, hIn, hOut, count, repeat);
        error |= runVariant<Pc2SwarMultiply>("pc2", queue, frameSpec, dIn, dOut, hIn, hOut, count, repeat);
        error |= runVariant<Pc3Kernighan>("pc3", queue, frameSpec, dIn, dOut, hIn, hOut, count, repeat);
        error |= runVariant<Pc4BitLoop>("pc4", queue, frameSpec, dIn, dOut, hIn, hOut, count, repeat);
        error |= runVariant<Pc5LookupTable>("pc5", queue, frameSpec, dIn, dOut, hIn, hOut, count, repeat);
        error |= runVariant<Pc6Intrinsic>("pc6", queue, frameSpec, dIn, dOut, hIn, hOut, count, repeat);
        return error == EXIT_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
    }
} // namespace popcountAlpaka

auto main(int argc, char* argv[]) -> int
{
    int count = popcount::defaultCount;
    int repeat = popcount::defaultRepeat;

    if(int const ret = popcount::parseCmd(argc, argv, count, repeat))
        return ret;

    using namespace alpaka;

    return onHost::executeForEachIfHasDevice(
        [=](auto const& backend)
        { return popcountAlpaka::example(onHost::makeDeviceSpec(backend), getExecutor(backend), count, repeat); },
        onHost::allBackends(onHost::enabledDeviceSpecs, exec::enabledExecutors));
}
