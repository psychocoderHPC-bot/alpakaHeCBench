/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the stencil1d benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 */

#include <cerrno>
#include <climits>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace stencil1d
{
    //! HeCBench default run is `./main 134217728 1000`; the small fast default fits the test suite.
    constexpr int defaultLength = 1 << 20;
    constexpr int defaultRepeat = 5;

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

        /** Parse a decimal string into @p value.
         *
         * @return @c true on a non-empty digit sequence that fits into @c int, @c false otherwise.
         */
        inline bool parseInt(char const* s, int& value)
        {
            if(!allDigits(s))
                return false;
            errno = 0;
            char* end = nullptr;
            long const parsed = std::strtol(s, &end, 10);
            if(errno == ERANGE || end == s || *end != '\0' || parsed > INT_MAX)
                return false;
            value = static_cast<int>(parsed);
            return true;
        }
    } // namespace detail

    /** Parse the command line.
     *
     * Accepts both the HeCBench positional form (`<length> <repeat>`) and the named form
     * (`--length N --repeat M`). Named options take precedence. The named form also accepts the
     * `--length=N` / `--repeat=M` spelling.
     *
     * `--all-backends` is an opt-in diagnostic flag and may appear anywhere on the command line. It
     * is removed before the positional and named forms are parsed.
     *
     * @return @c EXIT_SUCCESS on a valid command line, @c EXIT_FAILURE otherwise.
     */
    inline int parseCmd(int argc, char* argv[], int& length, int& repeat, bool& allBackends)
    {
        length = defaultLength;
        repeat = defaultRepeat;
        allBackends = false;

        // Drop the diagnostic flag so it does not disturb the positional/named grammar.
        std::vector<char const*> args;
        args.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
        for(int i = 1; i < argc; ++i)
        {
            if(std::strcmp(argv[i], "--all-backends") == 0)
                allBackends = true;
            else
                args.push_back(argv[i]);
        }
        int const numArgs = static_cast<int>(args.size());

        // HeCBench positional form `./main <length> <repeat>`.
        if(numArgs == 2 && detail::allDigits(args[0]) && detail::allDigits(args[1]))
        {
            int parsedLength = 0;
            int parsedRepeat = 0;
            if(!detail::parseInt(args[0], parsedLength) || !detail::parseInt(args[1], parsedRepeat))
            {
                std::cerr << "Error: length and repeat must be decimal integers.\n";
                return EXIT_FAILURE;
            }
            length = parsedLength;
            repeat = parsedRepeat;
            if(length <= 0 || repeat <= 0)
            {
                std::cerr << "Error: length and repeat must be greater than zero.\n";
                return EXIT_FAILURE;
            }
            return EXIT_SUCCESS;
        }

        // Named form. Both `--name value` and `--name=value` are accepted.
        for(int i = 0; i < numArgs; ++i)
        {
            std::string const arg = args[static_cast<std::size_t>(i)];

            // -1: error, 0: option not matched, 1: option consumed
            auto consume = [&](char const* name, int& target) -> int
            {
                std::string const option = name;
                std::string const assignment = option + "=";

                char const* value = nullptr;
                if(arg == option)
                {
                    if(i + 1 >= numArgs)
                    {
                        std::cerr << "Error: " << option << " requires a value.\n";
                        return -1;
                    }
                    value = args[static_cast<std::size_t>(++i)];
                }
                else if(arg.rfind(assignment, 0) == 0)
                {
                    value = arg.c_str() + assignment.size();
                }
                else
                {
                    return 0; // not this option
                }

                if(!detail::parseInt(value, target))
                {
                    std::cerr << "Error: " << option << " expects a decimal integer.\n";
                    return -1;
                }
                return 1; // consumed
            };

            int const lengthResult = consume("--length", length);
            if(lengthResult < 0)
                return EXIT_FAILURE;
            if(lengthResult > 0)
                continue;

            int const repeatResult = consume("--repeat", repeat);
            if(repeatResult < 0)
                return EXIT_FAILURE;
            if(repeatResult > 0)
                continue;

            std::cerr << "Error: unrecognized argument '" << arg << "'.\n";
            return EXIT_FAILURE;
        }

        if(length <= 0)
        {
            std::cerr << "Error: length must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(repeat <= 0)
        {
            std::cerr << "Error: repeat must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
} // namespace stencil1d
