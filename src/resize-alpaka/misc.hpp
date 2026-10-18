/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the resize benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no
 * parts of the HeCBench code base.
 */

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace resizeBench
{
    //! Default input/output sizes, matching the HeCBench reference `run` target `1920 1080 256 256`.
    constexpr std::uint32_t defaultInWidth = 1920u;
    constexpr std::uint32_t defaultInHeight = 1080u;
    constexpr std::uint32_t defaultOutWidth = 256u;
    constexpr std::uint32_t defaultOutHeight = 256u;
    //! Default number of images, matching the reference `run` target (`512` channels).
    constexpr std::uint32_t defaultChannels = 512u;
    //! Kernel passes per measured sample, comparable to the reference `<repeat>` argument.
    constexpr int defaultRepeat = 100;
    //! Number of independent measured samples.
    constexpr int defaultSamples = 5;

    struct Params
    {
        std::uint32_t inWidth = defaultInWidth;
        std::uint32_t inHeight = defaultInHeight;
        std::uint32_t outWidth = defaultOutWidth;
        std::uint32_t outHeight = defaultOutHeight;
        std::uint32_t channels = defaultChannels;
        int repeat = defaultRepeat;
        int samples = defaultSamples;
        //! Pixel type sizes to run: 1 (u8), 2 (u16), 4 (u32). Default: all three.
        std::vector<std::uint32_t> typeSizes{1u, 2u, 4u};
        //! Run the nearest kernel (default true).
        bool nearest = true;
        //! Run the bilinear kernel (default true).
        bool bilinear = true;
        //! Restrict the run to validation only (no timing loop).
        bool verifyOnly = false;

        [[nodiscard]] std::vector<bool> modes() const
        {
            std::vector<bool> m;
            if(nearest)
                m.push_back(false);
            if(bilinear)
                m.push_back(true);
            return m;
        }

        [[nodiscard]] std::vector<std::uint32_t> types() const
        {
            return typeSizes;
        }

        [[nodiscard]] std::string typeName() const
        {
            std::string s;
            for(std::size_t i = 0; i < typeSizes.size(); ++i)
                s += (i ? "," : "") + std::to_string(typeSizes[i] * 8u);
            return s;
        }

        [[nodiscard]] std::string modeName() const
        {
            std::string s;
            if(nearest)
                s += "nearest";
            if(bilinear)
                s += (s.empty() ? "" : ",") + std::string("bilinear");
            return s;
        }
    };

    namespace detail
    {
        inline bool parseUint32(std::string_view text, std::uint32_t& value)
        {
            if(text.empty() || text.front() == '-')
                return false;
            std::string const buffer{text};
            char* end = nullptr;
            errno = 0;
            unsigned long long const parsed = std::strtoull(buffer.c_str(), &end, 10);
            if(errno == ERANGE || end == buffer.c_str() || *end != '\0'
               || parsed > static_cast<unsigned long long>(UINT32_MAX))
                return false;
            value = static_cast<std::uint32_t>(parsed);
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
            std::uint32_t unusedUint = 0;
            int unusedInt = 0;
            return parseUint32(text, unusedUint) || parseInt(text, unusedInt);
        }

        inline void printUsage(char const* program)
        {
            std::cout
                << "Usage: " << program
                << " [<in_width> <in_height> <out_width> <out_height> <channels> <repeat>]\n"
                << "       " << program
                << " [--in-width W] [--in-height H] [--out-width W2] [--out-height H2]\n"
                << "       " << program
                << " [--channels C] [--repeat N] [--samples S] [--types T]...\n"
                << "       " << program
                << " [--nearest] [--bilinear] [--verify-only]\n"
                << "  <in_width> <in_height> <out_width> <out_height> <channels> <repeat>\n"
                << "                           HeCBench positional form\n"
                << "  --in-width W             Input image width in pixels\n"
                << "  --in-height H            Input image height in pixels\n"
                << "  --out-width W2           Output image width in pixels\n"
                << "  --out-height H2          Output image height in pixels\n"
                << "  --channels C             Number of single-channel images\n"
                << "  --repeat N               Kernel passes per measured sample\n"
                << "  --samples S              Independent measured samples\n"
                << "  --types T                Pixel type sizes in bytes (1,2,4); repeatable\n"
                << "  --nearest                Run the nearest-neighbour kernel\n"
                << "  --bilinear               Run the bilinear kernel\n"
                << "  --verify-only            Only run the correctness check, no timing\n"
                << "  -h, --help               Print this help\n";
        }
    } // namespace detail

    /** Parse the command line.
     *
     * Accepts the HeCBench positional form
     * `resize <in_width> <in_height> <out_width> <out_height> <channels> <repeat>` as well as named
     * options `--in-width W --in-height H --out-width W2 --out-height H2 --channels C --repeat N
     * --samples S --types T --nearest --bilinear --verify-only`. Options also accept the
     * `--name=value` spelling. The pixel type option is repeatable; the first occurrence replaces the
     * default set `1,2,4`.
     */
    inline int parseCmd(int argc, char* argv[], Params& params)
    {
        params = Params{};

        // Positional HeCBench form: six numeric arguments in reference order.
        if(argc == 7 && detail::isNumber(argv[1]) && detail::isNumber(argv[2]) && detail::isNumber(argv[3])
           && detail::isNumber(argv[4]) && detail::isNumber(argv[5]) && detail::isNumber(argv[6]))
        {
            if(!detail::parseUint32(argv[1], params.inWidth) || !detail::parseUint32(argv[2], params.inHeight)
               || !detail::parseUint32(argv[3], params.outWidth)
               || !detail::parseUint32(argv[4], params.outHeight)
               || !detail::parseUint32(argv[5], params.channels) || !detail::parseInt(argv[6], params.repeat))
            {
                std::cerr << "Error: invalid positional argument.\n";
                return EXIT_FAILURE;
            }
        }
        else
        {
            bool typesSeen = false;
            for(int i = 1; i < argc; ++i)
            {
                std::string_view const arg{argv[i]};

                if(arg == "-h" || arg == "--help")
                {
                    detail::printUsage(argv[0]);
                    std::exit(EXIT_SUCCESS);
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

                if(name == "--nearest" || name == "--bilinear" || name == "--verify-only")
                {
                    if(hasInlineValue)
                    {
                        std::cerr << "Error: '" << name << "' does not take a value.\n";
                        return EXIT_FAILURE;
                    }
                    if(name == "--nearest")
                        params.nearest = true;
                    else if(name == "--bilinear")
                        params.bilinear = true;
                    else
                        params.verifyOnly = true;
                    continue;
                }

                if(name != "--in-width" && name != "--in-height" && name != "--out-width"
                   && name != "--out-height" && name != "--channels" && name != "--repeat"
                   && name != "--samples" && name != "--types")
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

                if(name == "--repeat" || name == "--samples")
                {
                    int* target = name == "--repeat" ? &params.repeat : &params.samples;
                    if(!detail::parseInt(text, *target))
                    {
                        std::cerr << "Error: invalid value '" << text << "' for '" << name << "'.\n";
                        return EXIT_FAILURE;
                    }
                }
                else if(name == "--types")
                {
                    std::uint32_t typeSize = 0;
                    if(!detail::parseUint32(text, typeSize))
                    {
                        std::cerr << "Error: invalid value '" << text << "' for '--types'.\n";
                        return EXIT_FAILURE;
                    }
                    if(!typesSeen)
                    {
                        params.typeSizes.clear();
                        typesSeen = true;
                    }
                    params.typeSizes.push_back(typeSize);
                }
                else
                {
                    std::uint32_t* target = nullptr;
                    if(name == "--in-width")
                        target = &params.inWidth;
                    else if(name == "--in-height")
                        target = &params.inHeight;
                    else if(name == "--out-width")
                        target = &params.outWidth;
                    else if(name == "--out-height")
                        target = &params.outHeight;
                    else
                        target = &params.channels;
                    if(!detail::parseUint32(text, *target))
                    {
                        std::cerr << "Error: invalid value '" << text << "' for '" << name << "'.\n";
                        return EXIT_FAILURE;
                    }
                }
            }
        }

        if(params.inWidth == 0u || params.inHeight == 0u || params.outWidth == 0u || params.outHeight == 0u)
        {
            std::cerr << "Error: image dimensions must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(params.channels == 0u)
        {
            std::cerr << "Error: number of channels must be greater than zero.\n";
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
        if(params.typeSizes.empty())
        {
            std::cerr << "Error: at least one pixel type must be selected.\n";
            return EXIT_FAILURE;
        }
        for(std::uint32_t t : params.typeSizes)
        {
            if(t != 1u && t != 2u && t != 4u)
            {
                std::cerr << "Error: pixel type size must be 1, 2 or 4 bytes.\n";
                return EXIT_FAILURE;
            }
        }
        if(!params.nearest && !params.bilinear)
        {
            std::cerr << "Error: at least one of --nearest or --bilinear must be selected.\n";
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

} // namespace resizeBench
