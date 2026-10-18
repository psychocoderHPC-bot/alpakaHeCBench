/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the HeCBench `laplace3d` benchmark.
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

namespace laplace3d
{
    //! HeCBench `make run` uses `./main 128 128 128 100 1`; keep those as defaults.
    constexpr int defaultNx = 128;
    constexpr int defaultNy = 128;
    constexpr int defaultNz = 128;
    constexpr int defaultIters = 100;
    //! Number of measured timed runs; the HeCBench reference reports one value per invocation.
    constexpr int defaultRepeat = 5;

    //! Benchmark configuration handed to the example function.
    struct Config
    {
        int nx = defaultNx;
        int ny = defaultNy;
        int nz = defaultNz;
        int iters = defaultIters;
        int repeat = defaultRepeat;
        //! Set when the user asked for the usage text; parseCmd returns success and main exits.
        bool help = false;
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
            std::cout << "Usage: " << program << " [<Nx> <Ny> <Nz> <niter> [<repeat>]]\n"
                      << "       " << program
                      << " [--nx <n>] [--ny <n>] [--nz <n>] [--iters <n>] [--repeat <n>]\n"
                      << "  <Nx> <Ny> <Nz> <niter>  HeCBench positional form\n"
                      << "  --nx <n>                Grid size in x\n"
                      << "  --ny <n>                Grid size in y\n"
                      << "  --nz <n>                Grid size in z\n"
                      << "  --iters <n>             Number of Jacobi iterations\n"
                      << "  --repeat <n>            Number of measured timed runs\n"
                      << "  -h, --help              Print this help\n";
        }
    } // namespace detail

    /** Parse the command line.
     *
     * Accepts the HeCBench positional form (`<Nx> <Ny> <Nz> <niter> [<repeat>]`) and the named
     * form (`--nx N --ny N --nz N --iters N --repeat R`). Options also accept the `--name=value`
     * spelling. Unlike the reference, arbitrary dimensions are accepted: the reference rejects
     * non-multiples of its 32x8 launch block, which is a native-launch detail the portable alpaka
     * launch does not require.
     */
    inline int parseCmd(int argc, char* argv[], Config& cfg)
    {
        cfg = Config{};

        // Preserve the reference positional contract: Nx, Ny, Nz, niter, then an optional repeat.
        if(argc >= 5 && detail::allDigits(argv[1]) && detail::allDigits(argv[2]) && detail::allDigits(argv[3])
           && detail::allDigits(argv[4]))
        {
            cfg.nx = std::atoi(argv[1]);
            cfg.ny = std::atoi(argv[2]);
            cfg.nz = std::atoi(argv[3]);
            cfg.iters = std::atoi(argv[4]);
            if(argc >= 6)
            {
                if(!detail::allDigits(argv[5]))
                {
                    std::cerr << "Error: invalid positional repeat value '" << argv[5] << "'.\n";
                    return EXIT_FAILURE;
                }
                cfg.repeat = std::atoi(argv[5]);
            }
            if(argc > 6)
            {
                std::cerr << "Error: at most five positional arguments are accepted.\n";
                return EXIT_FAILURE;
            }
            if(cfg.nx <= 0 || cfg.ny <= 0 || cfg.nz <= 0 || cfg.iters < 0 || cfg.repeat <= 0)
            {
                std::cerr << "Error: Nx, Ny, Nz and repeat must be greater than zero; niter must not be negative.\n";
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
                cfg.help = true;
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
            if(name == "--nx")
                target = &cfg.nx;
            else if(name == "--ny")
                target = &cfg.ny;
            else if(name == "--nz")
                target = &cfg.nz;
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

        if(cfg.nx <= 0 || cfg.ny <= 0 || cfg.nz <= 0)
        {
            std::cerr << "Error: Nx, Ny and Nz must be greater than zero.\n";
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
} // namespace laplace3d
