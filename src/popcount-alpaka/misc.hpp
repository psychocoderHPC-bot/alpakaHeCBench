/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the popcount benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 *
 * The parser accepts the native HeCBench positional form `./main <count> <repeat>` and the
 * `--size <count>` / `--repeat <n>` options. It uses only the C++ standard library, so the benchmark
 * does not depend on Catch2.
 */

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace popcount
{
    // HeCBench runs the benchmark with `./main 16777216 1000`; use those values as defaults.
    constexpr int defaultCount = 16777216;
    constexpr int defaultRepeat = 1000;

    /** Parse one decimal integer without accepting trailing characters or overflow. */
    inline bool parseInteger(std::string_view text, int& value)
    {
        if(text.empty())
            return false;

        std::string const buffer{text};
        char* end = nullptr;
        errno = 0;
        long long const parsed = std::strtoll(buffer.c_str(), &end, 10);
        if(errno == ERANGE || end == buffer.c_str() || *end != '\0')
            return false;
        if(parsed < INT_MIN || parsed > INT_MAX)
            return false;

        value = static_cast<int>(parsed);
        return true;
    }

    inline void printUsage(char const* program)
    {
        std::cerr << "Usage: " << program << " [<count> [<repeat>]] [--size <count>] [--repeat <n>]\n";
    }

    /** Parse the command line and validate the parameters.
     *
     * Positional arguments fill `count` and then `repeat`; the equivalent options override neither,
     * so `./main --size 4096 --repeat 2` and `./main 4096 2` are both valid. Values must be positive.
     *
     * @return `EXIT_SUCCESS` when the arguments are valid, otherwise `EXIT_FAILURE`.
     */
    inline int parseCmd(int argc, char* argv[], int& count, int& repeat)
    {
        count = defaultCount;
        repeat = defaultRepeat;

        int positional = 0;
        bool optionsEnded = false;

        for(int i = 1; i < argc; ++i)
        {
            std::string_view const arg{argv[i]};

            if(!optionsEnded && arg == "--")
            {
                optionsEnded = true;
                continue;
            }

            if(!optionsEnded && arg.starts_with("--"))
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

                if(name != "--size" && name != "--repeat")
                {
                    std::cerr << "Error: unknown argument '" << arg << "'.\n";
                    printUsage(argv[0]);
                    return EXIT_FAILURE;
                }

                if(!hasInlineValue)
                {
                    if(i + 1 >= argc)
                    {
                        std::cerr << "Error: option '" << name << "' requires a value.\n";
                        printUsage(argv[0]);
                        return EXIT_FAILURE;
                    }
                    value = argv[++i];
                }

                int parsed = 0;
                if(!parseInteger(value, parsed))
                {
                    std::cerr << "Error: invalid value '" << value << "' for '" << name << "'.\n";
                    printUsage(argv[0]);
                    return EXIT_FAILURE;
                }

                if(name == "--size")
                    count = parsed;
                else
                    repeat = parsed;
                continue;
            }

            if(!optionsEnded && arg.size() > 1 && arg.front() == '-')
            {
                std::cerr << "Error: unknown argument '" << arg << "'.\n";
                printUsage(argv[0]);
                return EXIT_FAILURE;
            }

            int parsed = 0;
            if(!parseInteger(arg, parsed))
            {
                std::cerr << "Error: invalid positional value '" << arg << "'.\n";
                printUsage(argv[0]);
                return EXIT_FAILURE;
            }

            if(positional == 0)
                count = parsed;
            else if(positional == 1)
                repeat = parsed;
            else
            {
                std::cerr << "Error: too many positional arguments.\n";
                printUsage(argv[0]);
                return EXIT_FAILURE;
            }
            ++positional;
        }

        if(count <= 0)
        {
            std::cerr << "Error: number of elements must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(repeat <= 0)
        {
            std::cerr << "Error: number of repetitions must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        // A dedicated warm-up enqueue is performed before the measured iterations, see timeKernel().
        return EXIT_SUCCESS;
    }
} // namespace popcount
