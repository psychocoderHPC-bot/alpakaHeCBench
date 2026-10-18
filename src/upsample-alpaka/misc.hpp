/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the upsample benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 */

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace upsampleBench
{
    //! Default problem size: the HeCBench `run` target uses `32 64 64 64 100`.
    constexpr int defaultBatch = 32;
    constexpr int defaultChannels = 64;
    constexpr int defaultHeight = 64;
    constexpr int defaultWidth = 64;
    //! Reference `run` target repeats the pass 100 times per measurement.
    constexpr int defaultRepeat = 100;
    //! Number of independent measured samples.
    constexpr int defaultSamples = 5;
    //! Seed of the internal deterministic input generator (the reference calls `srand(0)`).
    constexpr std::uint64_t defaultSeed = 0u;
    /** Per-tile relative-L2 acceptance threshold.
     *
     * The forward pass is an exact 2x2 replication and the backward pass sums one 2x2 tile in a fixed
     * order, so a correct portable port reproduces the reference arithmetic exactly. The threshold
     * leaves room for a SIMD lane-reordering backend (one rounding unit, ~6e-8 for fp32) while still
     * rejecting any wrong tile mapping, border rule or offset. The per-tile metric is defined in
     * `helper.hpp`.
     */
    constexpr double defaultTolerance = 1e-8;
    //! Returned by parseCmd() after printing the help text. The caller exits without running.
    constexpr int parseHelp = 2;

    struct Params
    {
        int batch = defaultBatch;
        int channels = defaultChannels;
        int height = defaultHeight;
        int width = defaultWidth;
        int repeat = defaultRepeat;
        int samples = defaultSamples;
        std::uint64_t seed = defaultSeed;
        //! Restrict the run to validation only (no timing loop).
        bool verifyOnly = false;
    };

    namespace detail
    {
        /** Parse a complete base-10 integer, rejecting trailing characters and overflow. */
        inline bool parseInt(std::string_view text, int& value)
        {
            if(text.empty())
                return false;
            std::string const buffer{text};
            char* end = nullptr;
            errno = 0;
            long const parsed = std::strtol(buffer.c_str(), &end, 10);
            if(errno == ERANGE || end == buffer.c_str() || *end != '\0' || parsed < INT_MIN
               || parsed > INT_MAX)
                return false;
            value = static_cast<int>(parsed);
            return true;
        }

        /** Parse a complete unsigned base-10 integer, rejecting trailing characters and overflow. */
        inline bool parseU64(std::string_view text, std::uint64_t& value)
        {
            if(text.empty() || text.front() == '-')
                return false;
            std::string const buffer{text};
            char* end = nullptr;
            errno = 0;
            unsigned long long const parsed = std::strtoull(buffer.c_str(), &end, 10);
            if(errno == ERANGE || end == buffer.c_str() || *end != '\0')
                return false;
            value = static_cast<std::uint64_t>(parsed);
            return true;
        }

        inline void printUsage(char const* program)
        {
            std::cout
                << "Usage: " << program
                << " [<batch> <channels> <height> <width> <repeat>] [options]\n"
                << "  <batch>           Batch size N (HeCBench positional form, default 32)\n"
                << "  <channels>        Number of channels C (default 64)\n"
                << "  <height>          Input height H (default 64)\n"
                << "  <width>           Input width W (default 64)\n"
                << "  <repeat>          Passes per measured sample (default 100)\n"
                << "  --batch <n>       Batch size N (default 32)\n"
                << "  --channels <n>    Number of channels C (default 64)\n"
                << "  --height <n>      Input height H (default 64)\n"
                << "  --width <n>       Input width W (default 64)\n"
                << "  --repeat <n>      Passes per measured sample (default 100)\n"
                << "  --samples <n>     Independent measured samples (default 5)\n"
                << "  --seed <n>        Input seed (default 0)\n"
                << "  --verify-only     Only run the correctness check, no timing\n"
                << "  -h, --help        Print this help\n";
        }
    } // namespace detail

    /** Parse the command line.
     *
     * The reference takes positional arguments `<batch size> <number of channels> <height> <width>
     * <repeat>`. This port accepts the same positional form and named options, but not both for the
     * same field. It generates its input internally, so there is no input file argument.
     *
     * @return EXIT_SUCCESS on success, parseHelp after printing the help text, EXIT_FAILURE on a parse
     * or validation error.
     */
    inline int parseCmd(int argc, char* argv[], Params& params)
    {
        params = Params{};

        bool optionSet[5] = {false, false, false, false, false};
        int positional[5] = {0, 0, 0, 0, 0};
        int positionalCount = 0;

        for(int i = 1; i < argc; ++i)
        {
            std::string_view const arg{argv[i]};

            if(arg == "-h" || arg == "--help")
            {
                detail::printUsage(argv[0]);
                return parseHelp;
            }

            if(arg.rfind("--", 0) == 0)
            {
                std::string_view name = arg;
                std::string_view text;
                bool hasInlineValue = false;
                if(auto const separator = arg.find('='); separator != std::string_view::npos)
                {
                    name = arg.substr(0, separator);
                    text = arg.substr(separator + 1);
                    hasInlineValue = true;
                }

                if(name == "--verify-only")
                {
                    if(hasInlineValue)
                    {
                        std::cerr << "Error: option '" << name << "' does not take a value.\n";
                        return EXIT_FAILURE;
                    }
                    params.verifyOnly = true;
                    continue;
                }

                enum class Target
                {
                    batch,
                    channels,
                    height,
                    width,
                    repeat,
                    samples,
                    seed
                } target;

                int targetIndex = -1;
                if(name == "--batch")
                {
                    target = Target::batch;
                    targetIndex = 0;
                }
                else if(name == "--channels")
                {
                    target = Target::channels;
                    targetIndex = 1;
                }
                else if(name == "--height")
                {
                    target = Target::height;
                    targetIndex = 2;
                }
                else if(name == "--width")
                {
                    target = Target::width;
                    targetIndex = 3;
                }
                else if(name == "--repeat")
                {
                    target = Target::repeat;
                    targetIndex = 4;
                }
                else if(name == "--samples")
                    target = Target::samples;
                else if(name == "--seed")
                    target = Target::seed;
                else
                {
                    std::cerr << "Error: unknown option '" << name << "'.\n";
                    return EXIT_FAILURE;
                }

                if(!hasInlineValue)
                {
                    if(i + 1 >= argc)
                    {
                        std::cerr << "Error: missing value for '" << name << "'.\n";
                        return EXIT_FAILURE;
                    }
                    text = argv[++i];
                }

                bool valid = false;
                switch(target)
                {
                case Target::batch:
                    valid = detail::parseInt(text, params.batch);
                    break;
                case Target::channels:
                    valid = detail::parseInt(text, params.channels);
                    break;
                case Target::height:
                    valid = detail::parseInt(text, params.height);
                    break;
                case Target::width:
                    valid = detail::parseInt(text, params.width);
                    break;
                case Target::repeat:
                    valid = detail::parseInt(text, params.repeat);
                    break;
                case Target::samples:
                    valid = detail::parseInt(text, params.samples);
                    break;
                case Target::seed:
                    valid = detail::parseU64(text, params.seed);
                    break;
                }

                if(!valid)
                {
                    std::cerr << "Error: invalid value '" << text << "' for '" << name << "'.\n";
                    return EXIT_FAILURE;
                }
                if(targetIndex >= 0)
                    optionSet[targetIndex] = true;
                continue;
            }

            if(arg.size() > 1 && arg.front() == '-')
            {
                std::cerr << "Error: unknown option '" << arg << "'.\n";
                return EXIT_FAILURE;
            }

            // Positional arguments: the upstream `<batch> <channels> <height> <width> <repeat>` form.
            int value = 0;
            if(!detail::parseInt(arg, value))
            {
                std::cerr << "Error: invalid positional value '" << arg << "'.\n";
                return EXIT_FAILURE;
            }
            if(positionalCount >= 5)
            {
                std::cerr << "Error: more than five positional arguments are not supported.\n";
                return EXIT_FAILURE;
            }
            positional[positionalCount++] = value;
        }

        if(positionalCount != 0 && positionalCount != 5)
        {
            std::cerr << "Error: the positional form requires exactly "
                         "<batch> <channels> <height> <width> <repeat>.\n";
            return EXIT_FAILURE;
        }

        if(positionalCount == 5)
        {
            for(int k = 0; k < 5; ++k)
            {
                if(optionSet[k])
                {
                    std::cerr << "Error: a positional argument and the matching option both set the "
                                 "same value.\n";
                    return EXIT_FAILURE;
                }
            }
            params.batch = positional[0];
            params.channels = positional[1];
            params.height = positional[2];
            params.width = positional[3];
            params.repeat = positional[4];
        }

        if(params.batch <= 0)
        {
            std::cerr << "Error: batch must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(params.channels <= 0)
        {
            std::cerr << "Error: channels must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(params.height <= 0)
        {
            std::cerr << "Error: height must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(params.width <= 0)
        {
            std::cerr << "Error: width must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(params.repeat <= 0)
        {
            std::cerr << "Error: repeat must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(params.samples <= 0)
        {
            std::cerr << "Error: samples must be greater than zero.\n";
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

} // namespace upsampleBench
