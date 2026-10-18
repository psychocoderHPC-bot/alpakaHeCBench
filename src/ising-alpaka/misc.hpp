/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the ising benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 */

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace ising
{
    //! HeCBench defaults, matching `src/ising-cuda/main.cu`.
    constexpr int defaultNx = 5120;
    constexpr int defaultNy = 5120;
    constexpr int defaultNwarmup = 100;
    constexpr int defaultNiters = 1000;
    constexpr double defaultAlpha = 0.1;
    constexpr unsigned long long defaultSeed = 1234ULL;
    constexpr int defaultRepeat = 5;
    //! Returned by parseCmd() after printing the help text. The caller exits without running.
    constexpr int parseHelp = 2;

    struct Params
    {
        int nx = defaultNx;
        int ny = defaultNy;
        int nwarmup = defaultNwarmup;
        int niters = defaultNiters;
        int repeat = defaultRepeat;
        double alpha = defaultAlpha;
        unsigned long long seed = defaultSeed;
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

        /** Parse a complete base-10 floating-point value, rejecting trailing characters. */
        inline bool parseDouble(std::string_view text, double& value)
        {
            if(text.empty())
                return false;
            std::string const buffer{text};
            char* end = nullptr;
            errno = 0;
            double const parsed = std::strtod(buffer.c_str(), &end);
            if(errno == ERANGE || end == buffer.c_str() || *end != '\0')
                return false;
            value = parsed;
            return true;
        }

        /** Parse a complete base-10 unsigned value, rejecting trailing characters and overflow. */
        inline bool parseSeed(std::string_view text, unsigned long long& value)
        {
            if(text.empty() || text.front() == '-')
                return false;
            std::string const buffer{text};
            char* end = nullptr;
            errno = 0;
            unsigned long long const parsed = std::strtoull(buffer.c_str(), &end, 10);
            if(errno == ERANGE || end == buffer.c_str() || *end != '\0')
                return false;
            value = parsed;
            return true;
        }

        inline void printUsage(char const* program)
        {
            std::cout << "Usage: " << program << " [options]\n"
                      << "  --lattice-n <nx>  Number of lattice rows, even (default 5120)\n"
                      << "  --lattice-m <ny>  Number of lattice columns, even (default 5120)\n"
                      << "  --nwarmup <n>     Number of warm-up iterations (default 100)\n"
                      << "  --niters <n>      Number of trial iterations (default 1000)\n"
                      << "  --repeat <n>      Number of measured repetitions (default 5)\n"
                      << "  --alpha <a>       Coefficient of the critical temperature (default 0.1)\n"
                      << "  --seed <s>        Seed for the random number generation (default 1234)\n"
                      << "  -h, --help        Print this help\n";
        }
    } // namespace detail

    /** Parse the command line and validate the parameters.
     *
     * Accepts the named options of the reference port and the `--name=value` spelling. The
     * reference has no positional arguments. Error handling and defaults are unchanged.
     *
     * @return EXIT_SUCCESS on success, parseHelp after printing the help text, EXIT_FAILURE on a
     * parse or validation error.
     */
    inline int parseCmd(int argc, char* argv[], Params& params)
    {
        params = Params{};

        for(int i = 1; i < argc; ++i)
        {
            std::string_view const arg{argv[i]};

            if(arg == "-h" || arg == "--help")
            {
                detail::printUsage(argv[0]);
                return parseHelp;
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

            if(name != "--lattice-n" && name != "--lattice-m" && name != "--nwarmup" && name != "--niters"
               && name != "--repeat" && name != "--alpha" && name != "--seed")
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
            if(name == "--lattice-n")
                valid = detail::parseInt(text, params.nx);
            else if(name == "--lattice-m")
                valid = detail::parseInt(text, params.ny);
            else if(name == "--nwarmup")
                valid = detail::parseInt(text, params.nwarmup);
            else if(name == "--niters")
                valid = detail::parseInt(text, params.niters);
            else if(name == "--repeat")
                valid = detail::parseInt(text, params.repeat);
            else if(name == "--alpha")
                valid = detail::parseDouble(text, params.alpha);
            else
                valid = detail::parseSeed(text, params.seed);

            if(!valid)
            {
                std::cerr << "Error: invalid value '" << text << "' for '" << name << "'.\n";
                return EXIT_FAILURE;
            }
        }

        if(params.nx <= 0 || params.ny <= 0)
        {
            std::cerr << "Error: lattice dimensions must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(params.nx % 2 != 0 || params.ny % 2 != 0)
        {
            std::cerr << "Error: lattice dimensions must be even values.\n";
            return EXIT_FAILURE;
        }
        if(params.nwarmup < 0 || params.niters <= 0)
        {
            std::cerr << "Error: nwarmup must be non-negative and niters positive.\n";
            return EXIT_FAILURE;
        }
        if(params.repeat <= 0)
        {
            std::cerr << "Error: repeat must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(params.alpha <= 0.0)
        {
            std::cerr << "Error: alpha must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
} // namespace ising
