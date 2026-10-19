/* Copyright 2026 Rene Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the relu benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench/TensorFlow code base.
 */

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <string>

namespace relu
{
    constexpr int defaultCount = 10000000;
    constexpr int defaultRepeat = 100;

    /** Parse a signed decimal integer, rejecting empty input, trailing characters and overflow. */
    inline bool parseInteger(std::string const& text, int& out)
    {
        if(text.empty())
            return false;

        char* end = nullptr;
        errno = 0;
        long const value = std::strtol(text.c_str(), &end, 10);
        if(errno == ERANGE || end == text.c_str() || *end != '\0' || value < INT_MIN || value > INT_MAX)
            return false;
        out = static_cast<int>(value);
        return true;
    }

    /** Parse the command line and clamp the parameters to sane values.
     *
     * Accepts the named form `--size <count> --repeat <n>` and the native HeCBench positional form
     * `<count> <repeat>`. Named and positional arguments cannot be mixed. No arguments keeps the defaults.
     */
    inline int parseCmd(int argc, char* argv[], int& count, int& repeat)
    {
        count = defaultCount;
        repeat = defaultRepeat;

        bool sawOption = false;
        int positionals = 0;
        std::string positional[2];

        for(int i = 1; i < argc; ++i)
        {
            std::string const arg = argv[i];

            auto parseNamed = [&](std::string const& name) -> int
            {
                int* target = name == "--size" ? &count : &repeat;
                if(i + 1 >= argc)
                {
                    std::cerr << "Error: " << name << " requires a value.\n";
                    return EXIT_FAILURE;
                }
                int value = 0;
                if(!parseInteger(argv[++i], value))
                {
                    std::cerr << "Error: invalid value for " << name << ".\n";
                    return EXIT_FAILURE;
                }
                *target = value;
                return EXIT_SUCCESS;
            };

            if(arg == "--size" || arg == "--repeat")
            {
                sawOption = true;
                if(int const rc = parseNamed(arg))
                    return rc;
            }
            else if(arg.rfind("--size=", 0) == 0 || arg.rfind("--repeat=", 0) == 0)
            {
                sawOption = true;
                std::size_t const separator = arg.find('=');
                std::string const name = arg.substr(0, separator);
                int* target = name == "--size" ? &count : &repeat;
                int value = 0;
                if(!parseInteger(arg.substr(separator + 1), value))
                {
                    std::cerr << "Error: invalid value for " << name << ".\n";
                    return EXIT_FAILURE;
                }
                *target = value;
            }
            else if(!arg.empty() && arg[0] == '-')
            {
                std::cerr << "Error: unknown option '" << arg << "'.\n";
                return EXIT_FAILURE;
            }
            else
            {
                if(positionals < 2)
                    positional[positionals] = arg;
                ++positionals;
            }
        }

        if(sawOption)
        {
            if(positionals != 0)
            {
                std::cerr << "Error: positional arguments cannot be combined with --size/--repeat.\n";
                return EXIT_FAILURE;
            }
        }
        else if(positionals != 0)
        {
            if(positionals != 2)
            {
                std::cerr << "Error: expected two positional arguments <count> <repeat>.\n";
                return EXIT_FAILURE;
            }
            if(!parseInteger(positional[0], count) || !parseInteger(positional[1], repeat))
            {
                std::cerr << "Error: positional arguments must be integers.\n";
                return EXIT_FAILURE;
            }
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
} // namespace relu
