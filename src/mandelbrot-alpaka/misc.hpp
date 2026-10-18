/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the mandelbrot benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 */

#include "helper.hpp"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace mandel
{
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
            std::cout << "Usage: " << program << " [<repeat>] [options]\n"
                      << "  <repeat>          Number of measured repetitions (HeCBench positional form)\n"
                      << "  --rows <n>        Number of rows of the image\n"
                      << "  --cols <n>        Number of columns of the image\n"
                      << "  --max-iter <n>    Iteration limit\n"
                      << "  --repeat <n>      Number of measured repetitions\n"
                      << "  -h, --help        Print this help\n";
        }
    } // namespace detail

    /** Parse the command line for size, iteration limit and repetitions.
     *
     * Defaults replicate the HeCBench reference program (`row_size = 1024`, `col_size = 1024`,
     * `max_iterations = 1000`); the number of repetitions mirrors the reference `./main <repeat>`
     * argument. Both named options and the single positional `<repeat>` form are accepted.
     */
    inline int parseCmd(int argc, char* argv[], int& rows, int& cols, int& maxIterations, int& repetitions)
    {
        rows = defaultRowSize;
        cols = defaultColSize;
        maxIterations = defaultMaxIterations;
        repetitions = defaultRepetitions;

        bool hasPositional = false;
        bool hasRepeatOption = false;

        for(int i = 1; i < argc; ++i)
        {
            std::string_view const arg{argv[i]};

            if(arg == "-h" || arg == "--help")
            {
                detail::printUsage(argv[0]);
                return EXIT_SUCCESS;
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

                int* target = nullptr;
                if(name == "--rows")
                    target = &rows;
                else if(name == "--cols")
                    target = &cols;
                else if(name == "--max-iter")
                    target = &maxIterations;
                else if(name == "--repeat")
                {
                    target = &repetitions;
                    hasRepeatOption = true;
                }
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
                continue;
            }

            if(arg.size() > 1 && arg.front() == '-')
            {
                std::cerr << "Error: unknown option '" << arg << "'.\n";
                return EXIT_FAILURE;
            }

            if(hasPositional)
            {
                std::cerr << "Error: at most one positional <repeat> argument is accepted.\n";
                return EXIT_FAILURE;
            }
            if(!detail::parseInt(arg, repetitions))
            {
                std::cerr << "Error: invalid positional repeat value '" << arg << "'.\n";
                return EXIT_FAILURE;
            }
            hasPositional = true;
        }

        if(hasPositional && hasRepeatOption)
        {
            std::cerr << "Error: use either the positional <repeat> form or '--repeat', not both.\n";
            return EXIT_FAILURE;
        }

        if(rows <= 0 || cols <= 0)
        {
            std::cerr << "Error: rows and cols must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(maxIterations <= 0)
        {
            std::cerr << "Error: max iterations must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(repetitions <= 0)
        {
            std::cerr << "Error: repetitions must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
} // namespace mandel
