/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the black-scholes benchmark.
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

namespace blackScholesBench
{
    //! Reference `main` hardcodes 50,000,000 options per run.
    constexpr int defaultOptions = 50000000;
    //! Reference `run` target invokes `./main 100`.
    constexpr int defaultRepeat = 100;
    //! Number of independent measured samples.
    constexpr int defaultSamples = 5;
    //! Tolerance of the ported validation: the reference's own absolute tolerance `1.0e-4`
    //! against the published analytic prices.
    constexpr double defaultTolerance = 1.0e-4;
    //! Returned by parseCmd() after printing the help text. The caller exits without running.
    constexpr int parseHelp = 2;

    struct Params
    {
        int options = defaultOptions;
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

        inline void printUsage(char const* program)
        {
            std::cout
                << "Usage: " << program << " [<repeat>] [options]\n"
                << "  <repeat>          Kernel launches per measured sample (HeCBench positional\n"
                << "                    form; the reference invokes './main 100')\n"
                << "  --options <n>     Number of priced options (default 50000000)\n"
                << "  --repeat <n>      Kernel launches per measured sample (default 100)\n"
                << "  --samples <n>     Independent measured samples (default 5)\n"
                << "  --verify-only     Only run the correctness check, no timing\n"
                << "  -h, --help        Print this help\n";
        }
    } // namespace detail

    /** Parse the command line.
     *
     * The reference takes exactly one positional `<repeat>` and hardcodes the option count. This
     * port accepts the same positional repeat form plus named options, and generates the option
     * stream internally. Both the positional repeat and `--repeat` are accepted, but not at the
     * same time.
     *
     * @return EXIT_SUCCESS on success, parseHelp after printing the help text, EXIT_FAILURE on a
     * parse or validation error.
     */
    inline int parseCmd(int argc, char* argv[], Params& params)
    {
        params = Params{};

        bool hasPositionalRepeat = false;
        bool hasRepeatOption = false;

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
                    options,
                    repeat,
                    samples
                } target;

                if(name == "--options")
                    target = Target::options;
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
                case Target::options:
                    valid = detail::parseInt(text, params.options);
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

            // The only positional argument is the reference repeat count.
            int parsedRepeat = 0;
            if(!detail::parseInt(arg, parsedRepeat))
            {
                std::cerr << "Error: invalid positional repeat argument '" << arg << "'.\n";
                return EXIT_FAILURE;
            }
            if(hasPositionalRepeat)
            {
                std::cerr << "Error: more than one positional repeat argument is not supported.\n";
                return EXIT_FAILURE;
            }
            params.repeat = parsedRepeat;
            hasPositionalRepeat = true;
        }

        if(hasPositionalRepeat && hasRepeatOption)
        {
            std::cerr << "Error: use either the positional <repeat> form or '--repeat', not both.\n";
            return EXIT_FAILURE;
        }

        if(params.options <= 0)
        {
            std::cerr << "Error: options must be greater than zero.\n";
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

} // namespace blackScholesBench
