/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the morphology benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 */

#include "helper.hpp"

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace morphologyBench
{
    //! Default image size of the measured workload: the HeCBench-derived irregular 999x777 frame.
    constexpr int defaultWidth = 999;
    constexpr int defaultHeight = 777;
    //! Default structuring-element size (`hsize == vsize`); the measured workload uses ~70.
    constexpr int defaultSelSize = 71;
    //! Inner `dilate`+`erode` pairs per measured sample; the reference `run` target uses 100.
    constexpr int defaultRepeat = 100;
    //! Number of independent measured samples.
    constexpr int defaultSamples = 5;
    //! Zero selects the reference's single-white-pixel image; nonzero selects a deterministic random image.
    constexpr unsigned int defaultSeed = 0u;

    //! Largest structuring element the portable kernel supports (bound on the internal deque).
    constexpr int maxSelSize = 256;

    struct Params
    {
        int width = defaultWidth;
        int height = defaultHeight;
        int hsize = defaultSelSize;
        int vsize = defaultSelSize;
        int repeat = defaultRepeat;
        int samples = defaultSamples;
        unsigned int seed = defaultSeed;
        //! Restrict the run to validation only (no timing loop).
        bool verifyOnly = false;
        //! Set when the command line asked for usage information.
        bool helpRequested = false;
    };

    inline bool isPositive(int value)
    {
        return value > 0;
    }

    inline void printUsage(std::ostream& os)
    {
        os << "Usage: morphology [options]\n"
              "       morphology <kernel width> <kernel height> <image width> <image height> <repeat>\n"
              "\n"
              "Options:\n"
              "  --hsize <n>      Structuring-element width in pixels\n"
              "  --vsize <n>      Structuring-element height in pixels\n"
              "  --width <n>      Image width in pixels\n"
              "  --height <n>     Image height in pixels\n"
              "  --repeat <n>     Dilate+erode pairs per measured sample\n"
              "  --samples <n>    Independent measured samples\n"
              "  --seed <n>       0 = single-pixel image, else deterministic random image\n"
              "  --verify-only    Only run the correctness check, no timing\n"
              "  -h, --help       Print this help and exit\n"
              "\n"
              "The positional form matches the native HeCBench morphology binary. Named options and\n"
              "positional arguments cannot be mixed.\n";
    }

    inline bool parseSigned(std::string_view text, int& value)
    {
        if(text.empty())
            return false;
        if(text.front() == '+')
            text.remove_prefix(1);
        if(text.empty())
            return false;
        int parsed = 0;
        auto const* const begin = text.data();
        auto const* const end = begin + text.size();
        auto const [ptr, ec] = std::from_chars(begin, end, parsed);
        if(ec != std::errc{} || ptr != end)
            return false;
        value = parsed;
        return true;
    }

    inline bool parseUnsigned(std::string_view text, unsigned int& value)
    {
        if(text.empty() || text.front() == '-')
            return false;
        if(text.front() == '+')
            text.remove_prefix(1);
        if(text.empty())
            return false;
        unsigned long long parsed = 0;
        auto const* const begin = text.data();
        auto const* const end = begin + text.size();
        auto const [ptr, ec] = std::from_chars(begin, end, parsed);
        if(ec != std::errc{} || ptr != end || parsed > 0xffffffffull)
            return false;
        value = static_cast<unsigned int>(parsed);
        return true;
    }

    /** Parse the command line.
     *
     * Two forms are accepted:
     *  - named options, e.g. `--width 999 --height 777 --hsize 71 --vsize 71 --repeat 100`
     *    (`--option=value` is accepted as well);
     *  - the native HeCBench positional form
     *    `<kernel width> <kernel height> <image width> <image height> <repeat>`.
     *
     * The image is generated internally, so the port has no image-path argument. Defaults and
     * validation limits are unchanged from the source port. Returns EXIT_SUCCESS on success and
     * EXIT_FAILURE on a parse or validation error.
     */
    inline int parseCmd(int argc, char* argv[], Params& params)
    {
        bool namedOptionUsed = false;
        std::vector<std::string_view> positional;
        bool endOfOptions = false;

        auto fail = [&](std::string_view message) -> int
        {
            std::cerr << "Error: " << message << "\n";
            printUsage(std::cerr);
            return EXIT_FAILURE;
        };

        auto takeValue = [&](int& index, std::string_view& value) -> bool
        {
            if(index + 1 >= argc)
                return false;
            value = argv[++index];
            return true;
        };

        for(int i = 1; i < argc; ++i)
        {
            std::string_view const arg = argv[i];

            if(endOfOptions)
            {
                positional.push_back(arg);
                continue;
            }
            if(arg == "--")
            {
                endOfOptions = true;
                continue;
            }
            if(arg == "-h" || arg == "--help")
            {
                params.helpRequested = true;
                continue;
            }
            if(arg.size() < 2 || arg[0] != '-')
            {
                positional.push_back(arg);
                continue;
            }

            std::string_view name = arg;
            std::string_view value;
            bool hasValue = false;
            if(auto const eq = arg.find('='); eq != std::string_view::npos)
            {
                name = arg.substr(0, eq);
                value = arg.substr(eq + 1);
                hasValue = true;
            }

            auto requireValue = [&]() -> bool
            {
                if(hasValue)
                    return true;
                return takeValue(i, value);
            };

            namedOptionUsed = true;

            if(name == "--hsize")
            {
                if(!requireValue())
                    return fail("missing value for --hsize");
                if(!parseSigned(value, params.hsize))
                    return fail("--hsize must be an integer");
            }
            else if(name == "--vsize")
            {
                if(!requireValue())
                    return fail("missing value for --vsize");
                if(!parseSigned(value, params.vsize))
                    return fail("--vsize must be an integer");
            }
            else if(name == "--width")
            {
                if(!requireValue())
                    return fail("missing value for --width");
                if(!parseSigned(value, params.width))
                    return fail("--width must be an integer");
            }
            else if(name == "--height")
            {
                if(!requireValue())
                    return fail("missing value for --height");
                if(!parseSigned(value, params.height))
                    return fail("--height must be an integer");
            }
            else if(name == "--repeat")
            {
                if(!requireValue())
                    return fail("missing value for --repeat");
                if(!parseSigned(value, params.repeat))
                    return fail("--repeat must be an integer");
            }
            else if(name == "--samples")
            {
                if(!requireValue())
                    return fail("missing value for --samples");
                if(!parseSigned(value, params.samples))
                    return fail("--samples must be an integer");
            }
            else if(name == "--seed")
            {
                if(!requireValue())
                    return fail("missing value for --seed");
                if(!parseUnsigned(value, params.seed))
                    return fail("--seed must be an unsigned integer");
            }
            else if(name == "--verify-only")
            {
                if(hasValue)
                    return fail("--verify-only does not take a value");
                params.verifyOnly = true;
            }
            else
            {
                std::cerr << "Error: unknown option '" << name << "'\n";
                printUsage(std::cerr);
                return EXIT_FAILURE;
            }
        }

        if(params.helpRequested)
        {
            printUsage(std::cout);
            return EXIT_SUCCESS;
        }

        if(!positional.empty())
        {
            if(namedOptionUsed)
                return fail("positional arguments cannot be combined with named options");
            if(positional.size() != 5u)
                return fail("expected five positional arguments: <kernel width> <kernel height> "
                            "<image width> <image height> <repeat>");
            if(!parseSigned(positional[0], params.hsize))
                return fail("kernel width must be an integer");
            if(!parseSigned(positional[1], params.vsize))
                return fail("kernel height must be an integer");
            if(!parseSigned(positional[2], params.width))
                return fail("image width must be an integer");
            if(!parseSigned(positional[3], params.height))
                return fail("image height must be an integer");
            if(!parseSigned(positional[4], params.repeat))
                return fail("repeat must be an integer");
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
        if(!isPositive(params.hsize) || !isPositive(params.vsize))
        {
            std::cerr << "Error: hsize and vsize must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(params.hsize > maxSelSize || params.vsize > maxSelSize)
        {
            std::cerr << "Error: hsize and vsize must be <= " << maxSelSize << ".\n";
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

} // namespace morphologyBench
