/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the HeCBench `heat2d` benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 */

#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace heat2d
{
    //! HeCBench `make run` uses `./main 4096 4096 1000`; keep those as defaults.
    constexpr int defaultLx = 4096;
    constexpr int defaultLy = 4096;
    constexpr int defaultIters = 1000;
    //! Number of measured timed runs; the HeCBench reference reports one value per invocation.
    constexpr int defaultRepeat = 5;

    //! Benchmark configuration handed to the example function.
    struct Config
    {
        int lx = defaultLx;
        int ly = defaultLy;
        int iters = defaultIters;
        int repeat = defaultRepeat;
    };

    namespace detail
    {
        inline bool allDigits(char const* s)
        {
            if(s == nullptr || *s == '\0')
                return false;
            for(char const* p = s; *p != '\0'; ++p)
                if(!std::isdigit(static_cast<unsigned char>(*p)))
                    return false;
            return true;
        }

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
            std::cout << "Usage: " << program << " [<Lx> <Ly> <niter> [<repeat>]]\n"
                      << "       " << program << " [--lx <n>] [--ly <n>] [--iters <n>] [--repeat <n>]\n"
                      << "  <Lx> <Ly> <niter>  HeCBench positional form\n"
                      << "  --lx <n>           Grid size in x\n"
                      << "  --ly <n>           Grid size in y\n"
                      << "  --iters <n>        Number of heat-equation iterations\n"
                      << "  --repeat <n>       Number of measured timed runs\n"
                      << "  -h, --help         Print this help\n";
        }
    } // namespace detail

    /** Parse the command line.
     *
     * Accepts the HeCBench positional form (`<Lx> <Ly> <niter> [<repeat>]`) and the named form
     * (`--lx L --ly L --iters N --repeat R`). Options also accept the `--name=value` spelling.
     * Unlike the reference, arbitrary dimensions are accepted: the reference only rejects
     * non-multiples of its 16x16 block size for launch reasons, which the portable alpaka launch
     * does not require.
     */
    inline int parseCmd(int argc, char* argv[], Config& cfg)
    {
        cfg = Config{};

        // Preserve the reference positional contract: the first three arguments are Lx, Ly and
        // niter, with an optional fourth repeat argument.
        if(argc >= 4 && detail::allDigits(argv[1]) && detail::allDigits(argv[2]) && detail::allDigits(argv[3]))
        {
            cfg.lx = std::atoi(argv[1]);
            cfg.ly = std::atoi(argv[2]);
            cfg.iters = std::atoi(argv[3]);
            if(argc >= 5)
            {
                if(!detail::allDigits(argv[4]))
                {
                    std::cerr << "Error: invalid positional repeat value '" << argv[4] << "'.\n";
                    return EXIT_FAILURE;
                }
                cfg.repeat = std::atoi(argv[4]);
            }
            if(argc > 5)
            {
                std::cerr << "Error: at most four positional arguments are accepted.\n";
                return EXIT_FAILURE;
            }
            if(cfg.lx <= 0 || cfg.ly <= 0 || cfg.iters < 0 || cfg.repeat <= 0)
            {
                std::cerr << "Error: Lx, Ly and repeat must be greater than zero; niter must not be negative.\n";
                return EXIT_FAILURE;
            }
            return EXIT_SUCCESS;
        }

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

            int* target = nullptr;
            if(name == "--lx")
                target = &cfg.lx;
            else if(name == "--ly")
                target = &cfg.ly;
            else if(name == "--iters")
                target = &cfg.iters;
            else if(name == "--repeat")
                target = &cfg.repeat;
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

            if(!detail::parseInt(text, *target))
            {
                std::cerr << "Error: invalid value '" << text << "' for '" << name << "'.\n";
                return EXIT_FAILURE;
            }
        }

        if(cfg.lx <= 0 || cfg.ly <= 0)
        {
            std::cerr << "Error: Lx and Ly must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(cfg.iters < 0)
        {
            std::cerr << "Error: iters must not be negative.\n";
            return EXIT_FAILURE;
        }
        if(cfg.repeat <= 0)
        {
            std::cerr << "Error: repeat must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
} // namespace heat2d
