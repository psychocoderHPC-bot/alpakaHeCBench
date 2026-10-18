/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the bilateral filter benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 */

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace bilateralBench
{
    //! Default image size, matching the pinned HeCBench reference invocation (`main 2960 1440 0.5 0.5 1000`).
    constexpr int defaultWidth = 2960;
    constexpr int defaultHeight = 1440;
    //! Default range-kernel variance (third reference argument).
    constexpr float defaultVarianceI = 0.5f;
    //! Default spatial-kernel variance (fourth reference argument).
    constexpr float defaultVarianceSpatial = 0.5f;
    //! Inner filter passes per measured sample (fifth reference argument).
    constexpr int defaultRepeat = 1000;
    //! Number of independent measured samples.
    constexpr int defaultSamples = 5;
    //! Run every reference radius when `radius < 0`.
    constexpr int allRadii = -1;

    struct Params
    {
        int width = defaultWidth;
        int height = defaultHeight;
        float varianceI = defaultVarianceI;
        float varianceSpatial = defaultVarianceSpatial;
        int repeat = defaultRepeat;
        int samples = defaultSamples;
        //! Window radius; `< 0` runs the reference set {3, 6, 9}.
        int radius = allRadii;
        //! Restrict the run to validation only (no timing loop).
        bool verifyOnly = false;
    };

    inline void printUsage(std::ostream& os)
    {
        os << "Usage: bilateral [options]\n"
              "       bilateral <width> <height> <intensity> <spatial> <repeat>\n"
              "\n"
              "Options:\n"
              "  --width <pixels>       Image width (default: 2960)\n"
              "  --height <pixels>      Image height (default: 1440)\n"
              "  --intensity <variance> Range-kernel variance (default: 0.5)\n"
              "  --spatial <variance>   Spatial-kernel variance (default: 0.5)\n"
              "  --repeat <n>           Inner filter passes per measured sample (default: 1000)\n"
              "  --samples <n>          Independent measured samples (default: 5)\n"
              "  --radius <r>           Window radius, or -1 for the reference set {3,6,9} (default: -1)\n"
              "  --verify-only          Only run the correctness check, no timing\n"
              "  -h, --help             Print this help and exit\n";
    }

    namespace detail
    {
        inline bool parseInt(char const* text, int& value)
        {
            if(text == nullptr || *text == '\0')
                return false;
            errno = 0;
            char* end = nullptr;
            long const parsed = std::strtol(text, &end, 10);
            if(errno != 0 || end == text || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX)
                return false;
            value = static_cast<int>(parsed);
            return true;
        }

        inline bool parseFloat(char const* text, float& value)
        {
            if(text == nullptr || *text == '\0')
                return false;
            errno = 0;
            char* end = nullptr;
            float const parsed = std::strtof(text, &end);
            if(errno != 0 || end == text || *end != '\0')
                return false;
            value = parsed;
            return true;
        }
    } // namespace detail

    /** Parse the command line.
     *
     * The reference takes positional `<width> <height> <intensity> <spatial> <repeat>`. This port accepts that
     * form unchanged and also exposes named options plus an optional single radius for quick checks.
     * `--verify-only` skips timing and runs only the correctness check. Named options may appear before or
     * after the positional values and override them.
     */
    inline int parseCmd(int argc, char* argv[], Params& params)
    {
        int numPositional = 0;
        bool helpRequested = false;

        auto assignPositional = [&](char const* value) -> bool
        {
            switch(numPositional)
            {
            case 0:
                return detail::parseInt(value, params.width);
            case 1:
                return detail::parseInt(value, params.height);
            case 2:
                return detail::parseFloat(value, params.varianceI);
            case 3:
                return detail::parseFloat(value, params.varianceSpatial);
            case 4:
                return detail::parseInt(value, params.repeat);
            default:
                return false;
            }
        };

        for(int i = 1; i < argc; ++i)
        {
            std::string const arg = argv[i];

            if(arg == "-h" || arg == "--help")
            {
                helpRequested = true;
                continue;
            }
            if(arg == "--verify-only")
            {
                params.verifyOnly = true;
                continue;
            }

            if(arg.rfind("--", 0) == 0)
            {
                std::string name = arg;
                std::string value;
                bool hasValue = false;
                if(auto const eq = arg.find('='); eq != std::string::npos)
                {
                    name = arg.substr(0, eq);
                    value = arg.substr(eq + 1);
                    hasValue = true;
                }

                auto valueFor = [&](char const* option) -> char const*
                {
                    if(hasValue)
                        return value.c_str();
                    if(i + 1 >= argc)
                    {
                        std::cerr << "Error: option " << option << " requires a value.\n";
                        return nullptr;
                    }
                    return argv[++i];
                };

                if(name == "--width")
                {
                    char const* text = valueFor("--width");
                    if(text == nullptr || !detail::parseInt(text, params.width))
                    {
                        std::cerr << "Error: invalid value for --width.\n";
                        return EXIT_FAILURE;
                    }
                }
                else if(name == "--height")
                {
                    char const* text = valueFor("--height");
                    if(text == nullptr || !detail::parseInt(text, params.height))
                    {
                        std::cerr << "Error: invalid value for --height.\n";
                        return EXIT_FAILURE;
                    }
                }
                else if(name == "--intensity")
                {
                    char const* text = valueFor("--intensity");
                    if(text == nullptr || !detail::parseFloat(text, params.varianceI))
                    {
                        std::cerr << "Error: invalid value for --intensity.\n";
                        return EXIT_FAILURE;
                    }
                }
                else if(name == "--spatial")
                {
                    char const* text = valueFor("--spatial");
                    if(text == nullptr || !detail::parseFloat(text, params.varianceSpatial))
                    {
                        std::cerr << "Error: invalid value for --spatial.\n";
                        return EXIT_FAILURE;
                    }
                }
                else if(name == "--repeat")
                {
                    char const* text = valueFor("--repeat");
                    if(text == nullptr || !detail::parseInt(text, params.repeat))
                    {
                        std::cerr << "Error: invalid value for --repeat.\n";
                        return EXIT_FAILURE;
                    }
                }
                else if(name == "--samples")
                {
                    char const* text = valueFor("--samples");
                    if(text == nullptr || !detail::parseInt(text, params.samples))
                    {
                        std::cerr << "Error: invalid value for --samples.\n";
                        return EXIT_FAILURE;
                    }
                }
                else if(name == "--radius")
                {
                    char const* text = valueFor("--radius");
                    if(text == nullptr || !detail::parseInt(text, params.radius))
                    {
                        std::cerr << "Error: invalid value for --radius.\n";
                        return EXIT_FAILURE;
                    }
                }
                else
                {
                    std::cerr << "Error: unknown option '" << name << "'.\n";
                    return EXIT_FAILURE;
                }
                continue;
            }

            if(!assignPositional(argv[i]))
            {
                std::cerr << "Error: too many positional arguments or invalid positional value '" << arg << "'.\n";
                return EXIT_FAILURE;
            }
            ++numPositional;
        }

        if(helpRequested)
        {
            printUsage(std::cout);
            // Non-zero stops main() from running the benchmark; help is not a run.
            return EXIT_FAILURE;
        }

        if(params.width <= 0)
        {
            std::cerr << "Error: width must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(params.height <= 0)
        {
            std::cerr << "Error: height must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(!(params.varianceI > 0.0f))
        {
            std::cerr << "Error: intensity variance must be positive.\n";
            return EXIT_FAILURE;
        }
        if(!(params.varianceSpatial > 0.0f))
        {
            std::cerr << "Error: spatial variance must be positive.\n";
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
        if(params.radius >= 0 && params.radius > params.width - 1)
        {
            std::cerr << "Error: radius must not exceed width-1.\n";
            return EXIT_FAILURE;
        }
        if(params.radius >= 0 && params.radius > params.height - 1)
        {
            std::cerr << "Error: radius must not exceed height-1.\n";
            return EXIT_FAILURE;
        }
        if(params.radius < 0 && (params.width < 9 + 1 || params.height < 9 + 1))
        {
            std::cerr << "Error: width and height must be at least 10 for the reference radii {3,6,9}.\n";
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

} // namespace bilateralBench
