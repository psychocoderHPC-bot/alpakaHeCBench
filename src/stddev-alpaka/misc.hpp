/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the stddev benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 */

#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace stddevBench
{
    //! HeCBench default: number of columns (`D`), a multiple of 32 in the reference.
    constexpr uint32_t defaultD = 65536u;
    //! HeCBench default: number of rows (`N`).
    constexpr uint32_t defaultN = 16384u;
    //! HeCBench default: repeat count of the timed kernel pipeline.
    constexpr int defaultRepeat = 100;
    //! Independent measured samples per configuration.
    constexpr int defaultSamples = 5;
    //! Return value from parseCmd() when the caller asked for help and the usage text was printed.
    constexpr int helpRequested = -1;

    struct Params
    {
        uint32_t D = defaultD;
        uint32_t N = defaultN;
        int repeat = defaultRepeat;
        int samples = defaultSamples;
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

        inline bool parseU32(std::string const& text, uint32_t& value)
        {
            if(text.empty() || !allDigits(text.c_str()))
                return false;
            errno = 0;
            unsigned long const parsed = std::strtoul(text.c_str(), nullptr, 10);
            if(errno == ERANGE || parsed > std::numeric_limits<uint32_t>::max())
                return false;
            value = static_cast<uint32_t>(parsed);
            return true;
        }

        inline bool parseInt(std::string const& text, int& value)
        {
            if(text.empty() || !allDigits(text.c_str()))
                return false;
            errno = 0;
            long const parsed = std::strtol(text.c_str(), nullptr, 10);
            if(errno == ERANGE || parsed > std::numeric_limits<int>::max())
                return false;
            value = static_cast<int>(parsed);
            return true;
        }

        inline void printUsage(char const* program)
        {
            std::cout << "Usage: " << program << " [<D> <N> <repeat>] [options]\n"
                      << "  --D <columns>     Number of columns (matrix width), default " << defaultD << "\n"
                      << "  --N <rows>        Number of rows (matrix height), default " << defaultN << "\n"
                      << "  --repeat <n>      Kernel pipeline repetitions per sample, default " << defaultRepeat
                      << "\n"
                      << "  --samples <n>     Independent measured samples, default " << defaultSamples << "\n";
        }
    } // namespace detail

    /** Parse the command line.
     *
     * Accepts the HeCBench positional form `stddev_alpaka <D> <N> <repeat>` and the named form.
     * Named options take precedence when present.
     */
    inline int parseCmd(int argc, char* argv[], Params& params)
    {
        // Consume the HeCBench positional form before inspecting named options.
        if(argc == 4 && detail::allDigits(argv[1]) && detail::allDigits(argv[2]) && detail::allDigits(argv[3]))
        {
            params.D = static_cast<uint32_t>(std::strtoul(argv[1], nullptr, 10));
            params.N = static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 10));
            params.repeat = std::atoi(argv[3]);
        }
        else
        {
            for(int i = 1; i < argc; ++i)
            {
                std::string const token = argv[i];
                if(token == "-h" || token == "--help")
                {
                    detail::printUsage(argv[0]);
                    return helpRequested;
                }

                std::string name = token;
                std::string value;
                bool const hasInlineValue = token.find('=') != std::string::npos;
                if(hasInlineValue)
                {
                    auto const separator = token.find('=');
                    name = token.substr(0, separator);
                    value = token.substr(separator + 1);
                }

                auto nextValue = [&](std::string& out) -> bool
                {
                    if(hasInlineValue)
                    {
                        out = value;
                        return !out.empty();
                    }
                    if(i + 1 >= argc)
                        return false;
                    out = argv[++i];
                    return true;
                };

                std::string text;
                if(name == "--D")
                {
                    if(!nextValue(text))
                    {
                        std::cerr << "Error: --D requires a value.\n";
                        return EXIT_FAILURE;
                    }
                    if(!detail::parseU32(text, params.D))
                    {
                        std::cerr << "Error: invalid value '" << text << "' for --D.\n";
                        return EXIT_FAILURE;
                    }
                }
                else if(name == "--N")
                {
                    if(!nextValue(text))
                    {
                        std::cerr << "Error: --N requires a value.\n";
                        return EXIT_FAILURE;
                    }
                    if(!detail::parseU32(text, params.N))
                    {
                        std::cerr << "Error: invalid value '" << text << "' for --N.\n";
                        return EXIT_FAILURE;
                    }
                }
                else if(name == "--repeat")
                {
                    if(!nextValue(text))
                    {
                        std::cerr << "Error: --repeat requires a value.\n";
                        return EXIT_FAILURE;
                    }
                    if(!detail::parseInt(text, params.repeat))
                    {
                        std::cerr << "Error: invalid value '" << text << "' for --repeat.\n";
                        return EXIT_FAILURE;
                    }
                }
                else if(name == "--samples")
                {
                    if(!nextValue(text))
                    {
                        std::cerr << "Error: --samples requires a value.\n";
                        return EXIT_FAILURE;
                    }
                    if(!detail::parseInt(text, params.samples))
                    {
                        std::cerr << "Error: invalid value '" << text << "' for --samples.\n";
                        return EXIT_FAILURE;
                    }
                }
                else
                {
                    std::cerr << "Error: unknown argument '" << token << "'.\n";
                    detail::printUsage(argv[0]);
                    return EXIT_FAILURE;
                }
            }
        }

        if(params.D == 0u)
        {
            std::cerr << "Error: D (columns) must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(params.N < 2u)
        {
            std::cerr << "Error: N (rows) must be at least 2 for sample stddev.\n";
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
} // namespace stddevBench
