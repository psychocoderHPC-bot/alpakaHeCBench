/* Copyright 2026 Rene Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

/** @file Portable, single-source alpaka port of the HeCBench `cross` benchmark.
 *
 * The benchmark computes the 3D cross product of two dense tensors with shape `(numRows, 3)`. For every row
 * the output vector is `out = x1 x x2`. The same source is compiled for the host CPU and for a CUDA GPU; there
 * is no backend branch and no native CUDA/HIP/OpenMP kernel.
 *
 * The CLI, the stdout text, the input generation, the timing scope and the validation are aligned with the
 * native HeCBench implementations in `src/cross-cuda` and `src/cross-omp` at HeCBench commit
 * 0a17ee47ca6072192101036d4bf292e6612f22e6. Extended statistics are opt-in and go to stderr only.
 */

#include <alpaka/alpaka.hpp>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace cross
{
    //! Fallback values used by the extended command line mode.
    constexpr int defaultNumRows = 10'000'000;
    constexpr int defaultRepeat = 100;

    //! Number of rows one thread processes per index-map iteration.
    constexpr uint32_t chunkSize = 256u;

    //! Upper bound on the row count so that `numRows * 3` cannot overflow `int`.
    constexpr int maxNumRows = std::numeric_limits<int>::max() / 3;

    /** Print the native usage string to stdout. */
    inline void printUsage(char const* program)
    {
        printf("Usage: %s <number of rows in a 2D tensor> <repeat>\n", program);
    }

    /** cross1: torch-like strided formulation with inline element reads and a runtime stride.
     *
     * The expressions match the native `cross_kernel` in `cross-cuda/main.cu`. The stride is a runtime
     * parameter (1 in the benchmark), so the compiler cannot hoist the row components.
     */
    struct Cross1Kernel
    {
        ALPAKA_FN_ACC void operator()(
            auto const& acc,
            alpaka::concepts::IMdSpan auto out,
            alpaka::concepts::IMdSpan auto x1,
            alpaka::concepts::IMdSpan auto x2,
            int numRows,
            int stride) const
        {
            using namespace alpaka;

            for(auto i : onAcc::makeIdxMap(acc, onAcc::worker::linearThreadsInGrid, IdxRange{numRows}))
            {
                int const row = 3 * static_cast<int>(i);

                out[row + 0 * stride] = x1[row + 1 * stride] * x2[row + 2 * stride]
                                        - x1[row + 2 * stride] * x2[row + 1 * stride];
                out[row + 1 * stride] = x1[row + 2 * stride] * x2[row + 0 * stride]
                                        - x1[row + 0 * stride] * x2[row + 2 * stride];
                out[row + 2 * stride] = x1[row + 0 * stride] * x2[row + 1 * stride]
                                        - x1[row + 1 * stride] * x2[row + 0 * stride];
            }
        }
    };

    /** cross2: hoisted register loads with a runtime stride, matching the native `cross2_kernel`. */
    struct Cross2Kernel
    {
        ALPAKA_FN_ACC void operator()(
            auto const& acc,
            alpaka::concepts::IMdSpan auto out,
            alpaka::concepts::IMdSpan auto x1,
            alpaka::concepts::IMdSpan auto x2,
            int numRows,
            int stride) const
        {
            using namespace alpaka;
            using T = alpaka::GetValueType_t<ALPAKA_TYPEOF(x1)>;

            for(auto i : onAcc::makeIdxMap(acc, onAcc::worker::linearThreadsInGrid, IdxRange{numRows}))
            {
                int const row = 3 * static_cast<int>(i);

                T const x1_c0 = x1[row + 0 * stride];
                T const x1_c1 = x1[row + 1 * stride];
                T const x1_c2 = x1[row + 2 * stride];
                T const x2_c0 = x2[row + 0 * stride];
                T const x2_c1 = x2[row + 1 * stride];
                T const x2_c2 = x2[row + 2 * stride];

                out[row + 0 * stride] = x1_c1 * x2_c2 - x1_c2 * x2_c1;
                out[row + 1 * stride] = x1_c2 * x2_c0 - x1_c0 * x2_c2;
                out[row + 2 * stride] = x1_c0 * x2_c1 - x1_c1 * x2_c0;
            }
        }
    };

    /** cross3: hoisted register loads with a compile-time unit stride, matching the native `cross3_kernel`. */
    struct Cross3Kernel
    {
        ALPAKA_FN_ACC void operator()(
            auto const& acc,
            alpaka::concepts::IMdSpan auto out,
            alpaka::concepts::IMdSpan auto x1,
            alpaka::concepts::IMdSpan auto x2,
            int numRows) const
        {
            using namespace alpaka;
            using T = alpaka::GetValueType_t<ALPAKA_TYPEOF(x1)>;

            for(auto i : onAcc::makeIdxMap(acc, onAcc::worker::linearThreadsInGrid, IdxRange{numRows}))
            {
                int const row = 3 * static_cast<int>(i);

                T const x1_c0 = x1[row + 0];
                T const x1_c1 = x1[row + 1];
                T const x1_c2 = x1[row + 2];
                T const x2_c0 = x2[row + 0];
                T const x2_c1 = x2[row + 1];
                T const x2_c2 = x2[row + 2];

                out[row + 0] = x1_c1 * x2_c2 - x1_c2 * x2_c1;
                out[row + 1] = x1_c2 * x2_c0 - x1_c0 * x2_c2;
                out[row + 2] = x1_c0 * x2_c1 - x1_c1 * x2_c0;
            }
        }
    };

    /** Build a frame specification that gives every worker a useful amount of work. */
    template<typename T_Data>
    auto makeFrameSpec(auto const& device, auto const& exec, uint32_t extent)
    {
        using namespace alpaka;
        uint32_t const elementsPerWorker = getNumElemPerThread<T_Data>(device);
        uint32_t const blocks = alpaka::divCeil(std::max(extent, 1u), chunkSize * elementsPerWorker);
        return onHost::FrameSpec{std::max(blocks, 1u), chunkSize, exec};
    }

    //! Absolute tolerance used by the HeCBench reference check.
    template<typename T>
    constexpr double absTolerance()
    {
        if constexpr(std::is_same_v<T, float>)
            return 1e-3;
        else
            return 1e-9;
    }

    //! Relative tolerance added on top of the absolute tolerance to catch large-magnitude outputs.
    template<typename T>
    constexpr double relTolerance()
    {
        if constexpr(std::is_same_v<T, float>)
            return 1e-4;
        else
            return 0.0;
    }

    /** Independent double-precision reference for one 3-vector cross product from stored inputs. */
    inline void crossReference(double const* x1, double const* x2, double* out)
    {
        out[0] = x1[1] * x2[2] - x1[2] * x2[1];
        out[1] = x1[2] * x2[0] - x1[0] * x2[2];
        out[2] = x1[0] * x2[1] - x1[1] * x2[0];
    }

    /** Validate one kernel output against the double reference.
     *
     * Non-finite samples fail the check. Details go to stderr only when `verbose` is set, so the stdout byte
     * contract stays intact.
     *
     * @return EXIT_SUCCESS when all rows match, EXIT_FAILURE otherwise.
     */
    template<typename T>
    int verify(
        int numRows,
        char const* label,
        T const* out,
        std::vector<double> const& x1,
        std::vector<double> const& x2,
        bool verbose)
    {
        double const atol = absTolerance<T>();
        double const rtol = relTolerance<T>();
        int failures = 0;

        for(int i = 0; i < numRows; ++i)
        {
            double ref[3];
            crossReference(&x1[static_cast<std::size_t>(3 * i)], &x2[static_cast<std::size_t>(3 * i)], ref);

            for(int k = 0; k < 3; ++k)
            {
                double const got = static_cast<double>(out[3 * i + k]);
                if(!std::isfinite(got))
                {
                    if(verbose && failures < 5)
                        fprintf(
                            stderr,
                            "[extended] non-finite output in %s at row %d component %d: %g\n",
                            label,
                            i,
                            k,
                            got);
                    ++failures;
                    continue;
                }
                double const diff = std::fabs(got - ref[k]);
                double const tol = atol + rtol * std::fabs(ref[k]);
                if(diff > tol)
                {
                    if(verbose && failures < 5)
                        fprintf(
                            stderr,
                            "[extended] %s mismatch at row %d component %d: got %.9g expected %.9g "
                            "(diff %.3g > tol %.3g)\n",
                            label,
                            i,
                            k,
                            got,
                            ref[k],
                            diff,
                            tol);
                    ++failures;
                }
            }
        }

        return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    //! Compute median/min/max/standard deviation over a non-empty sample set.
    struct Stats
    {
        double median{0.0};
        double min{0.0};
        double max{0.0};
        double stddev{0.0};
    };

    inline Stats computeStats(std::vector<double> samples)
    {
        Stats s;
        if(samples.empty())
            return s;
        std::sort(samples.begin(), samples.end());
        s.min = samples.front();
        s.max = samples.back();
        s.median = samples[samples.size() / 2u];

        double mean = 0.0;
        for(double v : samples)
            mean += v;
        mean /= static_cast<double>(samples.size());

        double var = 0.0;
        for(double v : samples)
        {
            double const d = v - mean;
            var += d * d;
        }
        var /= static_cast<double>(samples.size());
        s.stddev = std::sqrt(var);
        return s;
    }

    /** Enqueue one kernel `repeat` times with a single timed interval and one final wait.
     *
     * Matches the native timing scope: no warm-up, one steady_clock around the whole loop, one
     * synchronization at the end. The reported value is the mean in microseconds.
     */
    template<typename T_KernelBundle>
    double timeKernelMean(auto& queue, auto const& frameSpec, T_KernelBundle const& kernel, int repeat)
    {
        using namespace alpaka;
        auto const start = std::chrono::steady_clock::now();
        for(int i = 0; i < repeat; ++i)
            queue.enqueue(frameSpec, kernel);
        onHost::wait(queue);
        auto const end = std::chrono::steady_clock::now();
        auto const totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        return static_cast<double>(totalNs) * 1e-3 / static_cast<double>(repeat);
    }

    /** Extended timing: one discarded warm-up, then per-iteration samples. Returns the statistics and the mean. */
    template<typename T_KernelBundle>
    std::pair<Stats, double> timeKernelExtended(
        auto& queue,
        auto const& frameSpec,
        T_KernelBundle const& kernel,
        int repeat)
    {
        using namespace alpaka;
        // discarded warm-up
        queue.enqueue(frameSpec, kernel);
        onHost::wait(queue);

        std::vector<double> samples;
        samples.reserve(static_cast<std::size_t>(repeat));
        double sum = 0.0;
        for(int i = 0; i < repeat; ++i)
        {
            auto const start = std::chrono::steady_clock::now();
            queue.enqueue(frameSpec, kernel);
            onHost::wait(queue);
            auto const end = std::chrono::steady_clock::now();
            double const us
                = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count())
                  * 1e-3;
            samples.push_back(us);
            sum += us;
        }
        double const mean = samples.empty() ? 0.0 : sum / static_cast<double>(samples.size());
        return {computeStats(std::move(samples)), mean};
    }

    //! Which data types an invocation runs.
    enum class DType
    {
        Float,
        Double,
        Both
    };

    /** Run both data types (or the one selected by `dtype`) for one accelerator. */
    template<typename T>
    int runType(
        auto const& deviceSpec,
        auto const& exec,
        int numRows,
        int repeat,
        bool extended)
    {
        using namespace alpaka;

        if(numRows < 1 || numRows > maxNumRows)
        {
            fprintf(stderr, "Error: number of rows must be in [1, %d].\n", maxNumRows);
            return EXIT_FAILURE;
        }

        auto devSelector = onHost::makeDeviceSelector(deviceSpec);
        onHost::Device device = devSelector.makeDevice(0);
        // The queue is intentionally non-blocking so the repeat loop times the enqueue path, as the native
        // implementation times the non-blocking kernel launches.
        onHost::Queue queue = device.makeQueue(queueKind::nonBlocking);

        int const numElems = numRows * 3;
        Vec<int, 1u> const extent(numElems);
        auto const numElemsU = static_cast<std::size_t>(numElems);

        // Single reproducible generator shared by the two input tensors, exactly like the native code.
        std::default_random_engine g(123);
        std::uniform_real_distribution<T> distr(-2.f, 2.f);
        std::vector<T> a(numElemsU);
        std::vector<T> b(numElemsU);
        for(std::size_t i = 0; i < numElemsU; ++i)
        {
            a[i] = distr(g);
            b[i] = distr(g);
        }

        // Independent double-precision reference inputs.
        std::vector<double> refA(numElemsU);
        std::vector<double> refB(numElemsU);
        for(std::size_t i = 0; i < numElemsU; ++i)
        {
            refA[i] = static_cast<double>(a[i]);
            refB[i] = static_cast<double>(b[i]);
        }

        auto hIn1 = onHost::allocHost<T>(extent);
        auto hIn2 = onHost::allocHost<T>(extent);
        auto hOut1 = onHost::allocHost<T>(extent);
        auto hOut2 = onHost::allocHost<T>(extent);
        auto hOut3 = onHost::allocHost<T>(extent);
        for(std::size_t i = 0; i < numElemsU; ++i)
        {
            hIn1[i] = a[i];
            hIn2[i] = b[i];
        }

        auto dIn1 = onHost::alloc<T>(device, extent);
        auto dIn2 = onHost::alloc<T>(device, extent);
        auto dOut = onHost::alloc<T>(device, extent);
        onHost::memcpy(queue, dIn1, hIn1);
        onHost::memcpy(queue, dIn2, hIn2);
        onHost::wait(queue);

        auto const frameSpec = makeFrameSpec<T>(device, exec, static_cast<uint32_t>(numRows));

        int result = EXIT_SUCCESS;

        // cross1: strided/torch-like, inline reads, runtime stride.
        {
            auto const kernel = KernelBundle{Cross1Kernel{}, dOut, dIn1, dIn2, numRows, 1};
            double const mean = timeKernelMean(queue, frameSpec, kernel, repeat);
            printf("Average execution time of cross1 kernel: %f (us)\n", mean);
            onHost::memcpy(queue, hOut1, dOut);
            onHost::wait(queue);
            if(extended)
            {
                auto const [stats, extMean] = timeKernelExtended(queue, frameSpec, kernel, repeat);
                fprintf(
                    stderr,
                    "[extended] cross1 median %.6f min %.6f max %.6f stddev %.6f mean %.6f (us)\n",
                    stats.median,
                    stats.min,
                    stats.max,
                    stats.stddev,
                    extMean);
            }
        }

        // cross2: hoisted register loads, runtime stride.
        {
            auto const kernel = KernelBundle{Cross2Kernel{}, dOut, dIn1, dIn2, numRows, 1};
            double const mean = timeKernelMean(queue, frameSpec, kernel, repeat);
            printf("Average execution time of cross2 kernel: %f (us)\n", mean);
            onHost::memcpy(queue, hOut2, dOut);
            onHost::wait(queue);
            if(extended)
            {
                auto const [stats, extMean] = timeKernelExtended(queue, frameSpec, kernel, repeat);
                fprintf(
                    stderr,
                    "[extended] cross2 median %.6f min %.6f max %.6f stddev %.6f mean %.6f (us)\n",
                    stats.median,
                    stats.min,
                    stats.max,
                    stats.stddev,
                    extMean);
            }
        }

        // cross3: contiguous hoisted loads, unit stride.
        {
            auto const kernel = KernelBundle{Cross3Kernel{}, dOut, dIn1, dIn2, numRows};
            double const mean = timeKernelMean(queue, frameSpec, kernel, repeat);
            printf("Average execution time of cross3 kernel: %f (us)\n", mean);
            onHost::memcpy(queue, hOut3, dOut);
            onHost::wait(queue);
            if(extended)
            {
                auto const [stats, extMean] = timeKernelExtended(queue, frameSpec, kernel, repeat);
                fprintf(
                    stderr,
                    "[extended] cross3 median %.6f min %.6f max %.6f stddev %.6f mean %.6f (us)\n",
                    stats.median,
                    stats.min,
                    stats.max,
                    stats.stddev,
                    extMean);
            }
        }

        // Independent validation of all three outputs against the double reference.
        result |= verify<T>(numRows, "cross1", std::data(hOut1), refA, refB, extended);
        result |= verify<T>(numRows, "cross2", std::data(hOut2), refA, refB, extended);
        result |= verify<T>(numRows, "cross3", std::data(hOut3), refA, refB, extended);

        printf("%s\n", result == EXIT_SUCCESS ? "PASS" : "FAIL");
        return result;
    }

    /** Emit the header and run one dtype pass. Returns EXIT_SUCCESS/EXIT_FAILURE. */
    template<typename T>
    int runOneType(auto const& deviceSpec, auto const& exec, int numRows, int repeat, bool extended)
    {
        printf("=========== Data type is %s ==========\n", std::is_same_v<T, float> ? "FP32" : "FP64");
        return runType<T>(deviceSpec, exec, numRows, repeat, extended);
    }

    /** Run the selected dtypes for one selected accelerator. */
    int runExample(
        auto const& deviceSpec,
        auto const& exec,
        int numRows,
        int repeat,
        DType dtype,
        bool extended)
    {
        int result = EXIT_SUCCESS;
        if(dtype == DType::Float || dtype == DType::Both)
            result |= runOneType<float>(deviceSpec, exec, numRows, repeat, extended);
        if(dtype == DType::Double || dtype == DType::Both)
            result |= runOneType<double>(deviceSpec, exec, numRows, repeat, extended);
        return result;
    }

    //! Parsed command line.
    struct Options
    {
        int numRows{defaultNumRows};
        int repeat{defaultRepeat};
        DType dtype{DType::Both};
        bool extended{false};
    };

    //! Parse an integer strictly, rejecting trailing garbage and out-of-range values.
    bool parsePositiveInt(char const* text, int& out)
    {
        char* end = nullptr;
        long const value = std::strtol(text, &end, 10);
        if(end == text || (end != nullptr && *end != '\0'))
            return false;
        if(value < 1 || value > std::numeric_limits<int>::max())
            return false;
        out = static_cast<int>(value);
        return true;
    }

    /** Parse the command line.
     *
     * @return EXIT_SUCCESS on success, EXIT_FAILURE when the usage/exit-1 path must be taken.
     */
    int parseCommandLine(int argc, char* argv[], Options& opts)
    {
        bool hasExtendedFlag = false;
        bool hasPositional = false;
        int positionals = 0;
        char const* positional[2] = {nullptr, nullptr};

        for(int i = 1; i < argc; ++i)
        {
            std::string const arg = argv[i];
            if(arg == "--extended")
            {
                hasExtendedFlag = true;
                opts.extended = true;
            }
            else if(arg == "--dtype")
            {
                hasExtendedFlag = true;
                if(i + 1 >= argc)
                    return EXIT_FAILURE;
                std::string const value = argv[++i];
                if(value == "float")
                    opts.dtype = DType::Float;
                else if(value == "double")
                    opts.dtype = DType::Double;
                else if(value == "both")
                    opts.dtype = DType::Both;
                else
                    return EXIT_FAILURE;
            }
            else if(arg == "--size")
            {
                hasExtendedFlag = true;
                if(i + 1 >= argc || !parsePositiveInt(argv[++i], opts.numRows))
                    return EXIT_FAILURE;
            }
            else if(arg == "--repeat")
            {
                hasExtendedFlag = true;
                if(i + 1 >= argc || !parsePositiveInt(argv[++i], opts.repeat))
                    return EXIT_FAILURE;
            }
            else if(!arg.empty() && arg[0] == '-')
            {
                // Unknown flag.
                return EXIT_FAILURE;
            }
            else
            {
                // Positional argument, only valid in the native two-argument interface.
                hasPositional = true;
                if(positionals < 2)
                    positional[positionals] = argv[i];
                ++positionals;
            }
        }

        if(hasExtendedFlag)
        {
            // Extended mode is selected exclusively by the named flags; positional arguments are rejected.
            if(hasPositional)
                return EXIT_FAILURE;
            if(opts.numRows < 1 || opts.numRows > maxNumRows)
                return EXIT_FAILURE;
            return EXIT_SUCCESS;
        }

        // Native interface: exactly two positional integer arguments.
        if(positionals == 2 && positional[0] != nullptr && positional[1] != nullptr)
        {
            int numRows = 0;
            int repeat = 0;
            if(!parsePositiveInt(positional[0], numRows) || !parsePositiveInt(positional[1], repeat))
                return EXIT_FAILURE;
            if(numRows > maxNumRows)
                return EXIT_FAILURE;
            opts.numRows = numRows;
            opts.repeat = repeat;
            return EXIT_SUCCESS;
        }

        return EXIT_FAILURE;
    }
} // namespace cross

auto main(int argc, char* argv[]) -> int
{
    using namespace alpaka;

    cross::Options opts;
    if(cross::parseCommandLine(argc, argv, opts) != EXIT_SUCCESS)
    {
        cross::printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    // Compile-time list of device specs; the runtime selection prefers a GPU over the host CPU.
    constexpr auto backends = onHost::allBackends(onHost::enabledDeviceSpecs, exec::enabledExecutors);

    std::optional<int> exitCode;

    auto attempt = [&](auto const& backend)
    {
        if(exitCode.has_value())
            return;
        auto const deviceSpec = onHost::DeviceSpec{backend};
        // Pass 1 only considers non-host accelerators.
        if(deviceSpec.getApi() == api::host)
            return;
        auto devSelector = onHost::makeDeviceSelector(deviceSpec);
        if(!devSelector.isAvailable())
            return;
        exitCode = cross::runExample(
            deviceSpec,
            alpaka::getExecutor(backend),
            opts.numRows,
            opts.repeat,
            opts.dtype,
            opts.extended);
    };

    std::apply([&](auto const&... backend) { (attempt(backend), ...); }, backends);

    // Pass 2 falls back to the host CPU when no GPU device was available.
    if(!exitCode.has_value())
    {
        auto attemptHost = [&](auto const& backend)
        {
            if(exitCode.has_value())
                return;
            auto const deviceSpec = onHost::DeviceSpec{backend};
            if(deviceSpec.getApi() != api::host)
                return;
            auto devSelector = onHost::makeDeviceSelector(deviceSpec);
            if(!devSelector.isAvailable())
                return;
            exitCode = cross::runExample(
                deviceSpec,
                alpaka::getExecutor(backend),
                opts.numRows,
                opts.repeat,
                opts.dtype,
                opts.extended);
        };
        std::apply([&](auto const&... backend) { (attemptHost(backend), ...); }, backends);
    }

    if(!exitCode.has_value())
    {
        fprintf(stderr, "No supported accelerator device is available.\n");
        return EXIT_FAILURE;
    }

    return *exitCode;
}
