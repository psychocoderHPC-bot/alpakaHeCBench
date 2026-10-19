/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the reverse benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 *
 * The parser accepts the native HeCBench positional form `./main <iterations>` and the named form
 * `--iterations <n> --length <n> --repeat <n> [--all-backends]`. It uses only the C++ standard
 * library, so the benchmark does not depend on Catch2.
 */

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace reverse
{
    //! Number of elements reversed by the HeCBench reference (`const int len = 256`).
    constexpr int defaultLength = 256;
    //! Largest supported length; the shared staging buffer is sized for this bound.
    constexpr int maxLength = 4096;
    //! Number of iteration/test cases. The reference binary is run as `./main 100`.
    constexpr int defaultIterations = 100;
    //! Number of measured repetitions of the per-case reverse loop (one value per invocation).
    constexpr int defaultRepeat = 5;

    namespace detail
    {
        inline bool allDigits(char const* s)
        {
            if(s == nullptr || *s == '\0')
                return false;
            for(char const* p = s; *p != '\0'; ++p)
                if(*p < '0' || *p > '9')
                    return false;
            return true;
        }

        /** Parse one decimal integer, rejecting trailing characters and overflow. */
        inline bool parseInteger(std::string_view text, int& value)
        {
            if(text.empty())
                return false;

            std::string const buffer{text};
            char* end = nullptr;
            errno = 0;
            long const parsed = std::strtol(buffer.c_str(), &end, 10);
            if(errno == ERANGE || end == buffer.c_str() || *end != '\0')
                return false;
            if(parsed < INT_MIN || parsed > INT_MAX)
                return false;

            value = static_cast<int>(parsed);
            return true;
        }

        inline void printUsage(char const* program)
        {
            std::cerr << "Usage: " << program
                      << " [<iterations>] [--iterations <n>] [--length <n>] [--repeat <n>]"
                         " [--all-backends]\n";
        }
    } // namespace detail

    /** Parse the command line.
     *
     * Accepts the HeCBench positional form (`./main <iterations>`) and the named form. Named options
     * take precedence when present. `--all-backends` opts into running every enabled backend and
     * executor; without it the benchmark selects exactly one backend and executor.
     *
     * @return `EXIT_SUCCESS` when the arguments are valid, otherwise `EXIT_FAILURE`.
     */
    inline int parseCmd(int argc, char* argv[], int& iterations, int& length, int& repeat, bool& allBackends)
    {
        iterations = defaultIterations;
        length = defaultLength;
        repeat = defaultRepeat;
        allBackends = false;

        // Consume the HeCBench positional form `./main <iterations>` before option parsing.
        if(argc == 2 && detail::allDigits(argv[1]))
        {
            if(!detail::parseInteger(argv[1], iterations) || iterations <= 0)
            {
                std::cerr << "Error: iterations must be greater than zero.\n";
                return EXIT_FAILURE;
            }
            return EXIT_SUCCESS;
        }

        for(int i = 1; i < argc; ++i)
        {
            std::string_view const arg{argv[i]};
            if(arg == "--help" || arg == "-h")
            {
                detail::printUsage(argv[0]);
                return EXIT_SUCCESS;
            }

            if(arg == "--all-backends")
            {
                allBackends = true;
                continue;
            }

            if(arg.starts_with("--"))
            {
                std::string_view name = arg;
                std::string_view value;
                bool hasInlineValue = false;
                if(auto const separator = arg.find('='); separator != std::string_view::npos)
                {
                    name = arg.substr(0, separator);
                    value = arg.substr(separator + 1);
                    hasInlineValue = true;
                }

                if(name != "--iterations" && name != "--length" && name != "--repeat")
                {
                    std::cerr << "Error: unknown option '" << arg << "'.\n";
                    detail::printUsage(argv[0]);
                    return EXIT_FAILURE;
                }

                if(!hasInlineValue)
                {
                    if(i + 1 >= argc)
                    {
                        std::cerr << "Error: option '" << name << "' requires a value.\n";
                        detail::printUsage(argv[0]);
                        return EXIT_FAILURE;
                    }
                    value = argv[++i];
                }

                int parsed = 0;
                if(!detail::parseInteger(value, parsed))
                {
                    std::cerr << "Error: invalid value '" << value << "' for '" << name << "'.\n";
                    detail::printUsage(argv[0]);
                    return EXIT_FAILURE;
                }

                if(name == "--iterations")
                    iterations = parsed;
                else if(name == "--length")
                    length = parsed;
                else
                    repeat = parsed;
                continue;
            }

            if(arg.size() > 1 && arg.front() == '-')
            {
                std::cerr << "Error: unknown option '" << arg << "'.\n";
                detail::printUsage(argv[0]);
                return EXIT_FAILURE;
            }

            std::cerr << "Error: unexpected positional argument '" << arg << "'.\n";
            detail::printUsage(argv[0]);
            return EXIT_FAILURE;
        }

        if(iterations <= 0)
        {
            std::cerr << "Error: iterations must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(length <= 0)
        {
            std::cerr << "Error: length must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(length > maxLength)
        {
            std::cerr << "Error: length must not exceed " << maxLength << ".\n";
            return EXIT_FAILURE;
        }
        if(repeat <= 0)
        {
            std::cerr << "Error: repeat must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
} // namespace reverse
