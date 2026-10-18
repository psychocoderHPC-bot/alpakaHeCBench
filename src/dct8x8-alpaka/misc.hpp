/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the dct8x8 benchmark.
 * This file contains no parts of the HeCBench code base.
 */

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace dct8x8Bench
{
    //! Default image size of the measured workload: the HeCBench `run` target uses 8192x8192.
    constexpr int defaultWidth = 8192;
    constexpr int defaultHeight = 8192;
    //! Reference `run` target transforms the image 100 times per direction.
    constexpr int defaultRepeat = 100;
    //! Number of independent measured samples.
    constexpr int defaultSamples = 5;
    //! Seed of the pinned reference `main` (`srand(2009)`).
    constexpr unsigned int defaultSeed = 2009u;
    //! The pinned reference transform definition is a relative-L2 comparison against a CPU
    //! reference with threshold `1e-6`; the same threshold is kept here.
    constexpr double defaultTolerance = 1e-6;

    struct Params
    {
        int width = defaultWidth;
        int height = defaultHeight;
        int repeat = defaultRepeat;
        int samples = defaultSamples;
        unsigned int seed = defaultSeed;
        //! Restrict the run to validation only (no timing loop).
        bool verifyOnly = false;
        //! Forward DCT (666 in the reference) unless set to inverse (777).
        bool inverse = false;
    };

    namespace detail
    {
        inline void usage(char const* program)
        {
            std::cout << "Usage: " << program
                      << " [--width W] [--height H] [--repeat R] [--samples S] [--seed X]"
                         " [--inverse] [--verify-only] [W H R]\n"
                         "  W H R  positional form: image width, image height and transforms per sample\n";
        }

        inline bool parseInt(std::string_view text, int& value)
        {
            if(text.empty())
                return false;
            int parsed = 0;
            auto const result = std::from_chars(text.data(), text.data() + text.size(), parsed);
            if(result.ec != std::errc{} || result.ptr != text.data() + text.size())
                return false;
            value = parsed;
            return true;
        }

        inline bool parseUnsigned(std::string_view text, unsigned int& value)
        {
            if(text.empty())
                return false;
            unsigned int parsed = 0;
            auto const result = std::from_chars(text.data(), text.data() + text.size(), parsed);
            if(result.ec != std::errc{} || result.ptr != text.data() + text.size())
                return false;
            value = parsed;
            return true;
        }
    } // namespace detail

    /** Parse the command line.
     *
     * Both the named options and the reference's positional form `<width> <height> <repeat>` are
     * accepted. The port generates the image internally; `--inverse` selects the inverse transform,
     * matching the second half of the reference `main`.
     */
    inline int parseCmd(int argc, char* argv[], Params& params)
    {
        std::vector<std::string_view> positional;
        bool widthSet = false;
        bool heightSet = false;
        bool repeatSet = false;

        for(int i = 1; i < argc; ++i)
        {
            std::string_view arg{argv[i]};

            if(arg == "-h" || arg == "--help")
            {
                detail::usage(argv[0]);
                return EXIT_SUCCESS;
            }

            if(arg.starts_with("--"))
            {
                std::string_view name = arg;
                std::string_view value;
                bool hasInlineValue = false;
                if(auto const pos = arg.find('='); pos != std::string_view::npos)
                {
                    name = arg.substr(0, pos);
                    value = arg.substr(pos + 1);
                    hasInlineValue = true;
                }

                if(name == "--inverse" || name == "--verify-only")
                {
                    if(hasInlineValue)
                    {
                        std::cerr << "Error: option " << name << " does not take a value.\n";
                        return EXIT_FAILURE;
                    }
                    if(name == "--inverse")
                        params.inverse = true;
                    else
                        params.verifyOnly = true;
                    continue;
                }

                bool const isValueOption
                    = name == "--width" || name == "--height" || name == "--repeat" || name == "--samples"
                      || name == "--seed";
                if(!isValueOption)
                {
                    std::cerr << "Error: unknown option '" << name << "'.\n";
                    return EXIT_FAILURE;
                }

                if(!hasInlineValue)
                {
                    if(i + 1 >= argc)
                    {
                        std::cerr << "Error: option " << name << " requires a value.\n";
                        return EXIT_FAILURE;
                    }
                    value = argv[++i];
                }

                bool ok = false;
                if(name == "--width")
                {
                    ok = detail::parseInt(value, params.width);
                    widthSet = true;
                }
                else if(name == "--height")
                {
                    ok = detail::parseInt(value, params.height);
                    heightSet = true;
                }
                else if(name == "--repeat")
                {
                    ok = detail::parseInt(value, params.repeat);
                    repeatSet = true;
                }
                else if(name == "--samples")
                    ok = detail::parseInt(value, params.samples);
                else
                    ok = detail::parseUnsigned(value, params.seed);

                if(!ok)
                {
                    std::cerr << "Error: invalid value for " << name << ": '" << value << "'.\n";
                    return EXIT_FAILURE;
                }
                continue;
            }

            if(arg.starts_with("-") && arg.size() > 1)
            {
                std::cerr << "Error: unknown option '" << arg << "'.\n";
                return EXIT_FAILURE;
            }

            positional.push_back(arg);
        }

        if(positional.size() > 3)
        {
            std::cerr << "Error: too many positional arguments (expected at most width height repeat).\n";
            return EXIT_FAILURE;
        }
        if(positional.size() >= 1 && widthSet)
        {
            std::cerr << "Error: width specified both positionally and with --width.\n";
            return EXIT_FAILURE;
        }
        if(positional.size() >= 2 && heightSet)
        {
            std::cerr << "Error: height specified both positionally and with --height.\n";
            return EXIT_FAILURE;
        }
        if(positional.size() >= 3 && repeatSet)
        {
            std::cerr << "Error: repeat specified both positionally and with --repeat.\n";
            return EXIT_FAILURE;
        }

        bool ok = true;
        if(positional.size() >= 1)
            ok = detail::parseInt(positional[0], params.width) && ok;
        if(positional.size() >= 2)
            ok = detail::parseInt(positional[1], params.height) && ok;
        if(positional.size() >= 3)
            ok = detail::parseInt(positional[2], params.repeat) && ok;
        if(!ok)
        {
            std::cerr << "Error: positional arguments must be integers.\n";
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

} // namespace dct8x8Bench
