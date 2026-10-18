/* Copyright 2026 Rene Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the axpby benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 */

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace axpby
{
    //! Default tensor count used by the HeCBench reference run (`./main 1000`).
    constexpr int defaultNumTensors = 1000;
    //! Elements per chunk. Mirrors the reference `BLOCK_SIZE` (512) at its first chunk size.
    constexpr int defaultChunkSize = 512;
    constexpr int defaultRepeat = 5;

    //! Returned by parseCmd() after printing the help text. The caller exits successfully without running.
    constexpr int parseHelp = -1;

    inline void printUsage(char const* program)
    {
        std::cout
            << "Usage: " << program
            << " [--tensors <n>] [--chunk-size <n>] [--repeat <n>] [--fixed-length <n>] [<tensors>]\n"
               "  --tensors <n>       Number of independent tensors (default "
            << defaultNumTensors
            << ")\n"
               "  --chunk-size <n>    Elements processed per chunk, one chunk per block (default "
            << defaultChunkSize
            << ")\n"
               "  --repeat <n>        Number of measured repetitions (default "
            << defaultRepeat
            << ")\n"
               "  --fixed-length <n>  Force this length for every tensor (0: use the HeCBench srand(123) "
               "lengths)\n"
               "  <tensors>           Positional form of --tensors, matching the native HeCBench binary\n"
               "  -h, --help          Print this help and exit\n";
    }

    /** Parse a whole decimal integer without accepting trailing characters. */
    inline bool parseInteger(std::string_view text, int& value)
    {
        if(text.empty())
            return false;

        std::string const copy{text};
        char* end = nullptr;
        errno = 0;
        long const parsed = std::strtol(copy.c_str(), &end, 10);
        if(errno == ERANGE || end == copy.c_str() || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX)
            return false;

        value = static_cast<int>(parsed);
        return true;
    }

    /** Parse the command line and validate the parameters.
     *
     * Supported forms are the long options below (with either `--name value` or `--name=value`), an optional
     * positional tensor count matching the native HeCBench invocation, and `-h`/`--help`.
     *
     * @param fixedLength if greater than zero every tensor has exactly this length, otherwise the length is drawn
     *                    from the HeCBench `srand(123)` sequence.
     * @return EXIT_SUCCESS on success, parseHelp after printing the help text, EXIT_FAILURE on a parse or
     *         validation error.
     */
    inline int parseCmd(
        int argc,
        char* argv[],
        int& numTensors,
        int& chunkSize,
        int& repeat,
        int& fixedLength)
    {
        numTensors = defaultNumTensors;
        chunkSize = defaultChunkSize;
        repeat = defaultRepeat;
        fixedLength = 0;

        bool havePositionalTensors = false;

        for(int i = 1; i < argc; ++i)
        {
            std::string const arg = argv[i];

            if(arg == "-h" || arg == "--help")
            {
                printUsage(argc > 0 ? argv[0] : "axpby");
                return parseHelp;
            }

            // Match `--name value` and `--name=value`. The return value is 1 when `arg` is a different option,
            // 0 when it was consumed, and -1 after an error has been reported.
            auto parseOption = [&](std::string_view name, int& destination) -> int
            {
                std::string_view value;
                auto const equal = arg.find('=');
                if(equal == std::string::npos)
                {
                    if(arg != name)
                        return 1;
                    if(i + 1 >= argc)
                    {
                        std::cerr << "Error: missing value for option '" << name << "'.\n";
                        return -1;
                    }
                    value = argv[++i];
                }
                else
                {
                    if(std::string_view{arg}.substr(0, equal) != name)
                        return 1;
                    value = std::string_view{arg}.substr(equal + 1);
                }

                if(!parseInteger(value, destination))
                {
                    std::cerr << "Error: invalid value '" << value << "' for option '" << name << "'.\n";
                    return -1;
                }
                return 0;
            };

            int consumed = parseOption("--tensors", numTensors);
            if(consumed == 1)
                consumed = parseOption("--chunk-size", chunkSize);
            if(consumed == 1)
                consumed = parseOption("--repeat", repeat);
            if(consumed == 1)
                consumed = parseOption("--fixed-length", fixedLength);

            if(consumed < 0)
                return EXIT_FAILURE;
            if(consumed == 0)
                continue;

            if(!arg.empty() && arg[0] != '-')
            {
                // Native HeCBench form: `./main <number of tensors>`.
                if(havePositionalTensors)
                {
                    std::cerr << "Error: more than one positional tensor count was given.\n";
                    return EXIT_FAILURE;
                }
                if(!parseInteger(arg, numTensors))
                {
                    std::cerr << "Error: invalid positional tensor count '" << arg << "'.\n";
                    return EXIT_FAILURE;
                }
                havePositionalTensors = true;
                continue;
            }

            std::cerr << "Error: unknown argument '" << arg << "'.\n";
            return EXIT_FAILURE;
        }

        if(numTensors <= 0)
        {
            std::cerr << "Error: number of tensors must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(chunkSize <= 0)
        {
            std::cerr << "Error: chunk size must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(repeat <= 0)
        {
            std::cerr << "Error: number of repetitions must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(fixedLength < 0)
        {
            std::cerr << "Error: fixed length must not be negative.\n";
            return EXIT_FAILURE;
        }
        if(fixedLength == 1)
        {
            std::cerr << "Error: fixed length must be either 0 or at least 2.\n";
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
} // namespace axpby
