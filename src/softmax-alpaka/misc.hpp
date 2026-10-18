/* Copyright 2026 Rene Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the softmax benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 */

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace softmax
{
    // Defaults from the HeCBench run recipe: `<numSlice> <sliceSize> <implementation> <repeat>` with
    // 100000 784 0 100 (see src/softmax-cuda/Makefile and benchmarks.yaml at the pinned SHA).
    constexpr int defaultNumSlice = 100'000;
    constexpr int defaultSliceSize = 784;
    constexpr int defaultRepeat = 100;

    /** Parse a decimal integer and require the whole token to be consumed. */
    inline bool parseInt(char const* text, int& value)
    {
        if(text == nullptr || *text == '\0')
            return false;
        errno = 0;
        char* end = nullptr;
        long const parsed = std::strtol(text, &end, 10);
        if(errno != 0 || end == text || *end != '\0')
            return false;
        value = static_cast<int>(parsed);
        return true;
    }

    /** Print the accepted command line forms. */
    inline void printUsage(char const* program)
    {
        std::cerr << "Usage: " << program << " <number of slices> <slice size> <implementation> <repeat>\n"
                  << "       " << program
                  << " [--size <rows>] [--slice-size <n>] [--repeat <n>] [--dtype <float|double|both>]\n"
                  << "implementation 0: naive (measured variant)\n";
    }

    /** Parse the command line and clamp the parameters to sane values.
     *
     * Both the native HeCBench positional form (`<numSlice> <sliceSize> <implementation> <repeat>`) and
     * the named options (`--size`, `--slice-size`, `--repeat`, `--dtype`) are accepted. The positional
     * implementation selector is parsed but ignored; the port implements the measured naive variant 0.
     */
    inline int parseCmd(int argc, char* argv[], int& numSlice, int& sliceSize, int& repeat, std::string& dtype)
    {
        numSlice = defaultNumSlice;
        sliceSize = defaultSliceSize;
        repeat = defaultRepeat;
        dtype = "float";

        std::vector<char const*> positional;
        for(int i = 1; i < argc; ++i)
        {
            std::string const arg = argv[i];
            if(arg == "-h" || arg == "--help")
            {
                printUsage(argv[0]);
                return EXIT_SUCCESS;
            }
            if(arg.rfind("--", 0) != 0)
            {
                positional.push_back(argv[i]);
                continue;
            }

            std::string name = arg.substr(2);
            std::string value;
            bool hasValue = false;
            if(auto const eq = name.find('='); eq != std::string::npos)
            {
                value = name.substr(eq + 1);
                name = name.substr(0, eq);
                hasValue = true;
            }

            if(name != "size" && name != "slice-size" && name != "repeat" && name != "dtype")
            {
                std::cerr << "Error: unknown option '" << arg << "'.\n";
                printUsage(argv[0]);
                return EXIT_FAILURE;
            }
            if(!hasValue)
            {
                if(i + 1 >= argc)
                {
                    std::cerr << "Error: option '--" << name << "' requires a value.\n";
                    printUsage(argv[0]);
                    return EXIT_FAILURE;
                }
                value = argv[++i];
            }

            if(name == "dtype")
            {
                dtype = value;
                continue;
            }

            int parsed = 0;
            if(!parseInt(value.c_str(), parsed))
            {
                std::cerr << "Error: option '--" << name << "' expects an integer, got '" << value << "'.\n";
                return EXIT_FAILURE;
            }
            if(name == "size")
                numSlice = parsed;
            else if(name == "slice-size")
                sliceSize = parsed;
            else
                repeat = parsed;
        }

        if(positional.size() > 4)
        {
            std::cerr << "Error: too many positional arguments.\n";
            printUsage(argv[0]);
            return EXIT_FAILURE;
        }
        for(std::size_t i = 0; i < positional.size(); ++i)
        {
            int parsed = 0;
            if(!parseInt(positional[i], parsed))
            {
                std::cerr << "Error: positional argument " << (i + 1) << " expects an integer, got '"
                          << positional[i] << "'.\n";
                return EXIT_FAILURE;
            }
            // The third positional argument selects the reference implementation (0 or 1); both compute
            // the same values and the port only measures variant 0, so the selector is ignored.
            if(i == 0)
                numSlice = parsed;
            else if(i == 1)
                sliceSize = parsed;
            else if(i == 3)
                repeat = parsed;
        }

        if(numSlice <= 0)
        {
            std::cerr << "Error: number of slices must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(sliceSize <= 0)
        {
            std::cerr << "Error: slice size must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(repeat <= 0)
        {
            std::cerr << "Error: number of repetitions must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
} // namespace softmax
