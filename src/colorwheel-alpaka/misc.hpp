/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the HeCBench `colorwheel` benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 */

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace colorwheel
{
    //! HeCBench reference run: `./main 10 4096 100`.
    constexpr float defaultTrueRange = 10.0f;
    constexpr int defaultSize = 4096;
    constexpr int defaultRepeat = 100;
    //! Number of independent measured samples.
    constexpr int defaultSamples = 5;

    struct Params
    {
        float trueRange = defaultTrueRange;
        int size = defaultSize;
        int repeat = defaultRepeat;
        int samples = defaultSamples;
        //! Restrict the run to validation only (no timing loop).
        bool verifyOnly = false;
    };

    namespace detail
    {
        inline bool parseFloat(std::string_view text, float& value)
        {
            if(text.empty())
                return false;
            std::string const buffer{text};
            char* end = nullptr;
            errno = 0;
            float const parsed = std::strtof(buffer.c_str(), &end);
            if(errno == ERANGE || end == buffer.c_str() || *end != '\0')
                return false;
            value = parsed;
            return true;
        }

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

        inline bool isNumber(std::string_view text)
        {
            float unusedFloat = 0.0f;
            int unusedInt = 0;
            return parseFloat(text, unusedFloat) || parseInt(text, unusedInt);
        }

        inline void printUsage(char const* program)
        {
            std::cout << "Usage: " << program << " [<range> <size> <repeat>]\n"
                      << "       " << program
                      << " [--range R] [--size N] [--repeat M] [--samples S] [--verify-only]\n"
                      << "  <range> <size> <repeat>  HeCBench positional form\n"
                      << "  --range R                Colour-wheel range\n"
                      << "  --size N                 Image side length in pixels\n"
                      << "  --repeat M               Kernel launches per measured sample\n"
                      << "  --samples S              Independent measured samples\n"
                      << "  --verify-only            Only run the correctness check, no timing\n"
                      << "  -h, --help               Print this help\n";
        }
    } // namespace detail

    /** Parse the command line.
     *
     * Accepts the HeCBench positional form `colorwheel_alpaka <range> <size> <repeat>` (this is what
     * the upstream `Makefile` `run` target uses) as well as named options
     * `--range R --size N --repeat M --samples S --verify-only`. Options also accept the
     * `--name=value` spelling.
     */
    inline int parseCmd(int argc, char* argv[], Params& params)
    {
        params = Params{};

        // Positional HeCBench form `<range> <size> <repeat>`.
        if(argc == 4 && detail::isNumber(argv[1]) && detail::isNumber(argv[2]) && detail::isNumber(argv[3]))
        {
            if(!detail::parseFloat(argv[1], params.trueRange) || !detail::parseInt(argv[2], params.size)
               || !detail::parseInt(argv[3], params.repeat))
            {
                std::cerr << "Error: invalid positional argument.\n";
                return EXIT_FAILURE;
            }
        }
        else
        {
            for(int i = 1; i < argc; ++i)
            {
                std::string_view const arg{argv[i]};

                if(arg == "-h" || arg == "--help")
                {
                    detail::printUsage(argv[0]);
                    return EXIT_SUCCESS;
                }

                if(arg.rfind("--", 0) != 0)
                {
                    std::cerr << "Error: unexpected positional argument '" << arg << "'.\n";
                    return EXIT_FAILURE;
                }

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
                        std::cerr << "Error: '--verify-only' does not take a value.\n";
                        return EXIT_FAILURE;
                    }
                    params.verifyOnly = true;
                    continue;
                }

                if(name != "--range" && name != "--size" && name != "--repeat" && name != "--samples")
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

                if(name == "--range")
                {
                    if(!detail::parseFloat(text, params.trueRange))
                    {
                        std::cerr << "Error: invalid value '" << text << "' for '--range'.\n";
                        return EXIT_FAILURE;
                    }
                }
                else
                {
                    int* target = name == "--size" ? &params.size
                                  : name == "--repeat" ? &params.repeat
                                                       : &params.samples;
                    if(!detail::parseInt(text, *target))
                    {
                        std::cerr << "Error: invalid value '" << text << "' for '" << name << "'.\n";
                        return EXIT_FAILURE;
                    }
                }
            }
        }

        if(params.trueRange <= 0.0f)
        {
            std::cerr << "Error: range must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(params.size < 3)
        {
            std::cerr << "Error: size must be at least 3 (a non-empty image with an interior).\n";
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

} // namespace colorwheel
