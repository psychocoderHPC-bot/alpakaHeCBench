/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the median filter benchmark.
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

namespace medianfilterBench
{
    //! Default image size, matching the HeCBench reference input (`SierrasRGB.ppm`, 1920x1080).
    constexpr std::uint32_t defaultWidth = 1920u;
    constexpr std::uint32_t defaultHeight = 1080u;
    //! Median filter passes per measured sample, comparable to the reference `<repeat>` argument.
    constexpr int defaultRepeat = 100;
    //! Number of independent measured samples.
    constexpr int defaultSamples = 5;
    //! Returned by parseCmd() after printing the help text. The caller exits successfully without running.
    constexpr int parseHelp = 2;

    struct Params
    {
        std::uint32_t width = defaultWidth;
        std::uint32_t height = defaultHeight;
        int repeat = defaultRepeat;
        int samples = defaultSamples;
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

        /** Parse a complete non-negative base-10 integer that fits into 32 bits. */
        inline bool parseU32(std::string_view text, std::uint32_t& value)
        {
            if(text.empty() || text.front() == '-')
                return false;
            std::string const buffer{text};
            char* end = nullptr;
            errno = 0;
            unsigned long const parsed = std::strtoul(buffer.c_str(), &end, 10);
            if(errno == ERANGE || end == buffer.c_str() || *end != '\0' || parsed > UINT32_MAX)
                return false;
            value = static_cast<std::uint32_t>(parsed);
            return true;
        }

        inline void printUsage(char const* program)
        {
            std::cout
                << "Usage: " << program << " [<image>] [<repeat>] [options]\n"
                << "  <image>           Upstream positional input path (accepted and ignored; the port\n"
                << "                    generates a deterministic RGBA image)\n"
                << "  <repeat>          Median filter passes per measured sample (HeCBench positional form)\n"
                << "  --width <n>       Image width in pixels (default 1920)\n"
                << "  --height <n>      Image height in pixels (default 1080)\n"
                << "  --repeat <n>      Median filter passes per measured sample (default 100)\n"
                << "  --samples <n>     Independent measured samples (default 5)\n"
                << "  --verify-only     Only run the correctness check, no timing\n"
                << "  -h, --help        Print this help\n";
        }
    } // namespace detail

    /** Parse the command line.
     *
     * The reference takes a positional `<PPM image> <repeat>`. This port generates its image internally,
     * so the image path is accepted for command-line compatibility and ignored. Named options override
     * the defaults. Both the upstream positional repeat form and `--repeat` are accepted, but not at the
     * same time. The pinned HeCBench reference is a fixed 3x3 filter; there is no window-size option in
     * the reference, so none is exposed here.
     *
     * @return EXIT_SUCCESS on success, parseHelp after printing the help text, EXIT_FAILURE on a parse
     * or validation error.
     */
    inline int parseCmd(int argc, char* argv[], Params& params)
    {
        params = Params{};

        bool hasPositionalRepeat = false;
        bool hasRepeatOption = false;
        bool hasImageArgument = false;

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
                    width,
                    height,
                    repeat,
                    samples
                } target;

                if(name == "--width")
                    target = Target::width;
                else if(name == "--height")
                    target = Target::height;
                else if(name == "--repeat")
                    target = Target::repeat;
                else if(name == "--samples")
                    target = Target::samples;
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
                case Target::width:
                    valid = detail::parseU32(text, params.width);
                    break;
                case Target::height:
                    valid = detail::parseU32(text, params.height);
                    break;
                case Target::repeat:
                    valid = detail::parseInt(text, params.repeat);
                    hasRepeatOption = true;
                    break;
                case Target::samples:
                    valid = detail::parseInt(text, params.samples);
                    break;
                }

                if(!valid)
                {
                    std::cerr << "Error: invalid value '" << text << "' for '" << name << "'.\n";
                    return EXIT_FAILURE;
                }
                continue;
            }

            if(arg.size() > 1 && arg.front() == '-')
            {
                std::cerr << "Error: unknown option '" << arg << "'.\n";
                return EXIT_FAILURE;
            }

            // Positional arguments: the optional upstream image path followed by the repeat count.
            int parsedRepeat = 0;
            if(detail::parseInt(arg, parsedRepeat))
            {
                if(hasPositionalRepeat)
                {
                    std::cerr << "Error: more than one positional repeat argument is not supported.\n";
                    return EXIT_FAILURE;
                }
                params.repeat = parsedRepeat;
                hasPositionalRepeat = true;
            }
            else if(!hasImageArgument && !hasPositionalRepeat)
            {
                hasImageArgument = true;
            }
            else
            {
                std::cerr << "Error: unexpected positional argument '" << arg << "'.\n";
                return EXIT_FAILURE;
            }
        }

        if(hasPositionalRepeat && hasRepeatOption)
        {
            std::cerr << "Error: use either the positional <repeat> form or '--repeat', not both.\n";
            return EXIT_FAILURE;
        }
        if(hasImageArgument)
        {
            std::cerr
                << "note: the positional image path is ignored; this port generates a deterministic image.\n";
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

} // namespace medianfilterBench
