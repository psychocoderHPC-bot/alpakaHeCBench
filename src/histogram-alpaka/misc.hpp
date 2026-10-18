/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the histogram benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 */

#include "helper.hpp"

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace histogramBench
{
    //! Default number of pixels; the HeCBench reference defaults to width=1920, height=1080 = 2,073,600.
    constexpr std::size_t defaultNumPixels = 1920u * 1080u;
    //! Inner histogram computations per measured sample, matching the reference `--i` default of 100.
    constexpr int defaultRepeat = 100;
    //! Number of independent measured samples.
    constexpr int defaultSamples = 5;
    //! Entropy reduction of the reference `--entropy` default.
    constexpr int defaultEntropy = 0;

    //! Returned by parseCmd() after printing the help text. The caller exits successfully without running.
    constexpr int parseHelp = -1;

    struct Params
    {
        //! Number of pixels (width * height in the reference).
        std::size_t numPixels = defaultNumPixels;
        int repeat = defaultRepeat;
        int samples = defaultSamples;
        int entropy = defaultEntropy;
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

        /** Parse a complete base-10 non-negative pixel count, rejecting trailing characters and overflow. */
        inline bool parseSize(std::string_view text, std::size_t& value)
        {
            if(text.empty() || text.front() == '-')
                return false;
            std::string const buffer{text};
            char* end = nullptr;
            errno = 0;
            unsigned long long const parsed = std::strtoull(buffer.c_str(), &end, 10);
            if(errno == ERANGE || end == buffer.c_str() || *end != '\0')
                return false;
            value = static_cast<std::size_t>(parsed);
            return true;
        }

        inline void printUsage(char const* program)
        {
            std::cout
                << "Usage: " << program
                << " [--pixels <n>] [--repeat <n>] [--samples <n>] [--entropy <n>] [--verify-only]\n"
                   "       "
                << program
                << " [--width <n>] [--height <n>] [--i <n>] [--samples <n>] [--entropy <n>] "
                   "[--verify-only]\n"
                   "  --pixels <n>     Number of pixels to histogram (default "
                << defaultNumPixels
                << ")\n"
                   "  --width <n>      Image width, multiplied by --height to set the pixel count (reference form)\n"
                   "  --height <n>     Image height, multiplied by --width to set the pixel count (reference form)\n"
                   "  --repeat <n>     Inner histogram computations per sample (default "
                << defaultRepeat
                << ")\n"
                   "  --i <n>          Reference alias for --repeat\n"
                   "  --samples <n>    Independent measured samples (default "
                << defaultSamples
                << ")\n"
                   "  --entropy <n>    Entropy reduction of the generated image (default "
                << defaultEntropy
                << ")\n"
                   "  --verify-only    Only run the correctness check, no timing\n"
                   "  -h, --help       Print this help and exit\n";
        }
    } // namespace detail

    /** Parse the command line.
     *
     * Long options accept both `--name value` and `--name=value`. The defaults match the HeCBench reference
     * (`width=1920`, `height=1080`, `--i=100`, `--entropy=0`). The reference `--width`/`--height` and `--i`
     * spellings are accepted alongside the port's `--pixels` and `--repeat`. The reference histogram has no
     * positional form, so a bare positional argument is rejected.
     *
     * @return EXIT_SUCCESS on success, parseHelp after printing the help text, EXIT_FAILURE on a parse or
     *         validation error.
     */
    inline int parseCmd(int argc, char* argv[], Params& params)
    {
        params = Params{};

        bool hasWidth = false;
        bool hasHeight = false;
        bool hasPixels = false;
        int width = 0;
        int height = 0;

        for(int i = 1; i < argc; ++i)
        {
            std::string const arg = argv[i];

            if(arg == "-h" || arg == "--help")
            {
                detail::printUsage(argc > 0 ? argv[0] : "histogram");
                return parseHelp;
            }

            if(arg == "--verify-only")
            {
                params.verifyOnly = true;
                continue;
            }

            // Match `--name value` and `--name=value`. The return value is 1 when `arg` is a different option,
            // 0 when it was consumed, and -1 after an error has been reported.
            auto optionValue = [&](std::string_view name, std::string_view& value) -> int
            {
                if(arg == name)
                {
                    if(i + 1 >= argc)
                    {
                        std::cerr << "Error: missing value for option '" << name << "'.\n";
                        return -1;
                    }
                    value = argv[++i];
                    return 0;
                }
                auto const equal = arg.find('=');
                if(equal != std::string::npos && std::string_view{arg}.substr(0, equal) == name)
                {
                    value = std::string_view{arg}.substr(equal + 1);
                    return 0;
                }
                return 1;
            };

            auto parseIntOption = [&](std::string_view name, int& destination) -> int
            {
                std::string_view value;
                int const consumed = optionValue(name, value);
                if(consumed != 0)
                    return consumed;
                if(!detail::parseInt(value, destination))
                {
                    std::cerr << "Error: invalid value '" << value << "' for option '" << name << "'.\n";
                    return -1;
                }
                return 0;
            };

            std::string_view value;
            int consumed = optionValue("--pixels", value);
            if(consumed == 0)
            {
                hasPixels = true;
                if(!detail::parseSize(value, params.numPixels))
                {
                    std::cerr << "Error: invalid value '" << value << "' for option '--pixels'.\n";
                    return EXIT_FAILURE;
                }
            }
            if(consumed == 1)
                consumed = parseIntOption("--repeat", params.repeat);
            if(consumed == 1)
                consumed = parseIntOption("--samples", params.samples);
            if(consumed == 1)
                consumed = parseIntOption("--entropy", params.entropy);
            if(consumed == 1)
                consumed = parseIntOption("--i", params.repeat);
            if(consumed == 1)
            {
                consumed = parseIntOption("--width", width);
                if(consumed == 0)
                    hasWidth = true;
            }
            if(consumed == 1)
            {
                consumed = parseIntOption("--height", height);
                if(consumed == 0)
                    hasHeight = true;
            }

            if(consumed < 0)
                return EXIT_FAILURE;
            if(consumed == 0)
                continue;

            if(!arg.empty() && arg.front() != '-')
            {
                std::cerr << "Error: unexpected positional argument '" << arg
                          << "'; use --pixels or --width/--height.\n";
                return EXIT_FAILURE;
            }

            std::cerr << "Error: unknown argument '" << arg << "'.\n";
            return EXIT_FAILURE;
        }

        if(hasWidth != hasHeight)
        {
            std::cerr << "Error: --width and --height must be given together.\n";
            return EXIT_FAILURE;
        }
        if(hasWidth && hasPixels)
        {
            std::cerr << "Error: use either --pixels or --width/--height, not both.\n";
            return EXIT_FAILURE;
        }
        if(hasWidth)
        {
            if(width <= 0 || height <= 0)
            {
                std::cerr << "Error: width and height must be greater than zero.\n";
                return EXIT_FAILURE;
            }
            params.numPixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        }

        if(params.numPixels == 0)
        {
            std::cerr << "Error: number of pixels must be greater than zero.\n";
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
        if(params.entropy < -1)
        {
            std::cerr << "Error: entropy must be >= -1.\n";
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

} // namespace histogramBench
