/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the HeCBench `gaussian` benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 */

#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace gaussianBench
{
    //! Default matrix size. The HeCBench `benchmarks.yaml` regression uses `-s 4096`.
    constexpr std::uint32_t defaultSize = 1024u;
    //! Inner forward eliminations per measured sample.
    constexpr int defaultRepeat = 1;
    //! Number of independent measured samples.
    constexpr int defaultSamples = 5;

    struct Params
    {
        //! Dimension of the square matrix / right-hand side.
        std::uint32_t size = defaultSize;
        //! Forward-elimination runs per measured sample.
        int repeat = defaultRepeat;
        //! Independent measured samples.
        int samples = defaultSamples;
        //! Restrict the run to the correctness check (no timing).
        bool verifyOnly = false;
        //! Print the full solution (debugging aid).
        bool verbose = false;
        //! Native-compatible quiet flag. The port never prints a matrix dump, so it only affects the timing line.
        bool quiet = false;
        //! Print the native-compatible "Device offloading time" line.
        bool timing = false;
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

        inline bool parseInteger(std::string const& text, int& value)
        {
            if(text.empty())
                return false;
            errno = 0;
            char* end = nullptr;
            long const parsed = std::strtol(text.c_str(), &end, 10);
            if(errno != 0 || end == text.c_str() || *end != '\0')
                return false;
            if(parsed < static_cast<long>(std::numeric_limits<int>::min())
               || parsed > static_cast<long>(std::numeric_limits<int>::max()))
                return false;
            value = static_cast<int>(parsed);
            return true;
        }

        inline bool parseSize(std::string const& text, std::uint32_t& value)
        {
            if(text.empty() || !std::isdigit(static_cast<unsigned char>(text.front())))
                return false;
            errno = 0;
            char* end = nullptr;
            unsigned long long const parsed = std::strtoull(text.c_str(), &end, 10);
            if(errno != 0 || end == text.c_str() || *end != '\0')
                return false;
            if(parsed > static_cast<unsigned long long>(std::numeric_limits<std::uint32_t>::max()))
                return false;
            value = static_cast<std::uint32_t>(parsed);
            return true;
        }

        /** Match the supported spelling of a long option and return its value.
         *
         * Accepts `--name value` and `--name=value`. Returns false when @p arg is not
         * this option. A missing value sets @p missing and returns true so the caller
         * can report the exact option.
         */
        inline bool takeOption(
            std::string const& arg,
            char const* name,
            int& index,
            int argc,
            char* argv[],
            std::string& value,
            bool& missing)
        {
            std::string const option = std::string("--") + name;
            if(arg == option)
            {
                if(index + 1 >= argc)
                {
                    missing = true;
                    return true;
                }
                value = argv[++index];
                return true;
            }
            std::string const prefix = option + "=";
            if(arg.compare(0, prefix.size(), prefix) == 0)
            {
                value = arg.substr(prefix.size());
                if(value.empty())
                    missing = true;
                return true;
            }
            return false;
        }

        /** Match a value-taking short option (`-s VALUE`). */
        inline bool takeShortOption(
            std::string const& arg,
            char name,
            int& index,
            int argc,
            char* argv[],
            std::string& value,
            bool& missing)
        {
            std::string const option(1, '-');
            std::string const shortOption = option + name;
            if(arg == shortOption)
            {
                if(index + 1 >= argc)
                {
                    missing = true;
                    return true;
                }
                value = argv[++index];
                return true;
            }
            return false;
        }
    } // namespace detail

    /** Parse the command line.
     *
     * Accepts a positional form (`<size> [repeat] [samples]`) and named options
     * (`--size`/`-s`, `--repeat`, `--samples`, `--verify-only`, `--verbose`). The native HeCBench
     * `-q` (quiet) and `-t` (timing) flags are accepted as well. Named options accept either a
     * separate value or `--option=value`.
     *
     * @return EXIT_SUCCESS on success, EXIT_FAILURE after printing a diagnostic.
     */
    inline int parseCmd(int argc, char* argv[], Params& params)
    {
        params = Params{};

        if(argc >= 2 && detail::allDigits(argv[1]))
        {
            params.size = static_cast<std::uint32_t>(std::strtoul(argv[1], nullptr, 10));
            for(int index = 2; index < argc; ++index)
            {
                if(!detail::allDigits(argv[index]))
                {
                    std::cerr << "Error: unexpected positional argument '" << argv[index] << "'.\n";
                    return EXIT_FAILURE;
                }
                if(index == 2)
                    params.repeat = std::atoi(argv[index]);
                else if(index == 3)
                    params.samples = std::atoi(argv[index]);
                else
                {
                    std::cerr << "Error: too many positional arguments.\n";
                    return EXIT_FAILURE;
                }
            }
            if(params.size == 0u)
            {
                std::cerr << "Error: matrix size must be greater than zero.\n";
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

        for(int index = 1; index < argc; ++index)
        {
            std::string const arg = argv[index];
            std::string value;
            bool missing = false;
            bool matched = false;

            matched = detail::takeOption(arg, "size", index, argc, argv, value, missing);
            if(!matched)
                matched = detail::takeShortOption(arg, 's', index, argc, argv, value, missing);
            if(matched)
            {
                if(missing)
                {
                    std::cerr << "Error: --size requires a value.\n";
                    return EXIT_FAILURE;
                }
                if(!detail::parseSize(value, params.size))
                {
                    std::cerr << "Error: --size expects a non-negative integer, got '" << value << "'.\n";
                    return EXIT_FAILURE;
                }
                continue;
            }

            matched = detail::takeOption(arg, "repeat", index, argc, argv, value, missing);
            if(matched)
            {
                if(missing)
                {
                    std::cerr << "Error: --repeat requires a value.\n";
                    return EXIT_FAILURE;
                }
                if(!detail::parseInteger(value, params.repeat))
                {
                    std::cerr << "Error: --repeat expects an integer, got '" << value << "'.\n";
                    return EXIT_FAILURE;
                }
                continue;
            }

            matched = detail::takeOption(arg, "samples", index, argc, argv, value, missing);
            if(matched)
            {
                if(missing)
                {
                    std::cerr << "Error: --samples requires a value.\n";
                    return EXIT_FAILURE;
                }
                if(!detail::parseInteger(value, params.samples))
                {
                    std::cerr << "Error: --samples expects an integer, got '" << value << "'.\n";
                    return EXIT_FAILURE;
                }
                continue;
            }

            if(arg == "--verify-only")
            {
                params.verifyOnly = true;
                continue;
            }
            if(arg == "--verbose")
            {
                params.verbose = true;
                continue;
            }
            if(arg == "-q" || arg == "--quiet")
            {
                params.quiet = true;
                continue;
            }
            if(arg == "-t" || arg == "--timing")
            {
                params.timing = true;
                continue;
            }

            std::cerr << "Error: unknown argument '" << arg << "'.\n";
            return EXIT_FAILURE;
        }

        if(params.size == 0u)
        {
            std::cerr << "Error: matrix size must be greater than zero.\n";
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
} // namespace gaussianBench
