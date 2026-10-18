/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the HeCBench `affine` benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 */

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <system_error>

namespace affineBench
{
    //! Default image width and height, matching the pinned HeCBench `X_SIZE`/`Y_SIZE` (512x512).
    constexpr std::uint32_t defaultWidth = 512u;
    constexpr std::uint32_t defaultHeight = 512u;
    //! Transform passes per measured sample.
    constexpr int defaultRepeat = 100;
    //! Number of independent measured samples.
    constexpr int defaultSamples = 5;

    struct Params
    {
        //! Image width in pixels.
        std::uint32_t width = defaultWidth;
        //! Image height in pixels.
        std::uint32_t height = defaultHeight;
        //! Transform passes per measured sample, comparable to the reference positional `<iterations>`.
        int repeat = defaultRepeat;
        //! Independent measured samples.
        int samples = defaultSamples;
        //! Restrict the run to the correctness check (no timing).
        bool verifyOnly = false;
    };

    namespace detail
    {
        inline bool parseUnsigned(std::string_view const text, std::uint32_t& value)
        {
            if(text.empty())
                return false;
            std::uint32_t parsed{};
            auto const* const end = text.data() + text.size();
            auto const result = std::from_chars(text.data(), end, parsed);
            if(result.ec != std::errc{} || result.ptr != end)
                return false;
            value = parsed;
            return true;
        }

        inline bool parseInt(std::string_view const text, int& value)
        {
            if(text.empty())
                return false;
            int parsed{};
            auto const* const end = text.data() + text.size();
            auto const result = std::from_chars(text.data(), end, parsed);
            if(result.ec != std::errc{} || result.ptr != end)
                return false;
            value = parsed;
            return true;
        }

        inline void printUsage(char const* program)
        {
            std::cout
                << "Usage: " << program
                << " [--width W] [--height H] [--repeat R] [--samples S] [--verify-only]\n"
                   "       "
                << program << " [W [H [R [S]]]]\n"
                               "\n"
                               "Options:\n"
                               "  --width W       Image width in pixels (default 512)\n"
                               "  --height H      Image height in pixels (default 512)\n"
                               "  --repeat R      Transform passes per measured sample (default 100)\n"
                               "  --samples S     Independent measured samples (default 5)\n"
                               "  --verify-only   Only run the correctness check, no timing\n"
                               "  -h, --help      Print this help\n"
                               "\n"
                               "The positional form takes width, height, repeat and samples in that order.\n";
        }
    } // namespace detail

    /** Parse the command line.
     *
     * The reference takes positional `<input image> <output image> <iterations>`. This port generates
     * its input internally, so it accepts named options and a positional shorthand for
     * `<width> <height> <repeat> <samples>`. The transform itself (rotation, scale and translation)
     * is fixed by the pinned reference and not configurable, so it is not exposed here.
     */
    inline int parseCmd(int argc, char* argv[], Params& params)
    {
        int positional = 0;

        for(int i = 1; i < argc; ++i)
        {
            std::string_view const arg{argv[i]};

            if(arg == "--help" || arg == "-h")
            {
                detail::printUsage(argv[0]);
                return EXIT_SUCCESS;
            }
            if(arg == "--verify-only")
            {
                params.verifyOnly = true;
                continue;
            }

            //! Match `--name value` and `--name=value`; return 1 on a match, 0 otherwise.
            auto readOption = [&](std::string_view const name, std::string_view& value) -> int
            {
                if(arg == name)
                {
                    if(i + 1 >= argc)
                    {
                        std::cerr << "Error: missing value for " << name << ".\n";
                        return -1;
                    }
                    value = argv[++i];
                    return 1;
                }
                if(arg.size() > name.size() && arg.substr(0, name.size()) == name && arg[name.size()] == '=')
                {
                    value = arg.substr(name.size() + 1);
                    if(value.empty())
                    {
                        std::cerr << "Error: missing value for " << name << ".\n";
                        return -1;
                    }
                    return 1;
                }
                return 0;
            };

            std::string_view value;

            if(int const matched = readOption("--width", value); matched != 0)
            {
                if(matched < 0)
                    return EXIT_FAILURE;
                if(!detail::parseUnsigned(value, params.width))
                {
                    std::cerr << "Error: invalid value for --width: " << value << "\n";
                    return EXIT_FAILURE;
                }
                continue;
            }
            if(int const matched = readOption("--height", value); matched != 0)
            {
                if(matched < 0)
                    return EXIT_FAILURE;
                if(!detail::parseUnsigned(value, params.height))
                {
                    std::cerr << "Error: invalid value for --height: " << value << "\n";
                    return EXIT_FAILURE;
                }
                continue;
            }
            if(int const matched = readOption("--repeat", value); matched != 0)
            {
                if(matched < 0)
                    return EXIT_FAILURE;
                if(!detail::parseInt(value, params.repeat))
                {
                    std::cerr << "Error: invalid value for --repeat: " << value << "\n";
                    return EXIT_FAILURE;
                }
                continue;
            }
            if(int const matched = readOption("--samples", value); matched != 0)
            {
                if(matched < 0)
                    return EXIT_FAILURE;
                if(!detail::parseInt(value, params.samples))
                {
                    std::cerr << "Error: invalid value for --samples: " << value << "\n";
                    return EXIT_FAILURE;
                }
                continue;
            }

            if(!arg.empty() && arg.front() == '-')
            {
                std::cerr << "Error: unknown option " << arg << ".\n";
                detail::printUsage(argv[0]);
                return EXIT_FAILURE;
            }

            switch(positional++)
            {
            case 0:
                if(!detail::parseUnsigned(arg, params.width))
                {
                    std::cerr << "Error: invalid positional value for width: " << arg << "\n";
                    return EXIT_FAILURE;
                }
                break;
            case 1:
                if(!detail::parseUnsigned(arg, params.height))
                {
                    std::cerr << "Error: invalid positional value for height: " << arg << "\n";
                    return EXIT_FAILURE;
                }
                break;
            case 2:
                if(!detail::parseInt(arg, params.repeat))
                {
                    std::cerr << "Error: invalid positional value for repeat: " << arg << "\n";
                    return EXIT_FAILURE;
                }
                break;
            case 3:
                if(!detail::parseInt(arg, params.samples))
                {
                    std::cerr << "Error: invalid positional value for samples: " << arg << "\n";
                    return EXIT_FAILURE;
                }
                break;
            default:
                std::cerr << "Error: too many positional arguments.\n";
                detail::printUsage(argv[0]);
                return EXIT_FAILURE;
            }
        }

        if(params.width == 0u)
        {
            std::cerr << "Error: width must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(params.height == 0u)
        {
            std::cerr << "Error: height must be greater than zero.\n";
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

} // namespace affineBench
