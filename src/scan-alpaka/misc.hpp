/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the `scan` benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 */

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace scanBench
{
    //! Default number of input elements; the HeCBench `run` target uses `268435456 100`.
    constexpr std::uint64_t defaultNumElements = 1ull << 26;
    //! Chunk size (elements per independent block scan); matches the reference default set.
    constexpr std::uint32_t defaultChunk = 2048u;
    //! Inner scans per measured sample.
    constexpr int defaultRepeat = 10;
    //! Number of independent measured samples.
    constexpr int defaultSamples = 5;

    struct Params
    {
        //! Number of input elements requested; the actual scanned count is rounded up per chunk.
        std::uint64_t numElements = defaultNumElements;
        //! Requested chunk size; 0 selects every reference chunk size (128, 256, 512, 1024, 2048).
        std::uint32_t chunk = defaultChunk;
        //! Inner scans per measured sample.
        int repeat = defaultRepeat;
        //! Independent measured samples.
        int samples = defaultSamples;
        //! Element type: "char", "short", "int", "long" or "all".
        std::string type = "int";
        //! Restrict the run to validation only (no timing loop).
        bool verifyOnly = false;
    };

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

        /** Parse a non-negative 64-bit integer without accepting signs or trailing garbage. */
        inline bool parseUint64(std::string const& text, std::uint64_t& out)
        {
            if(!allDigits(text.c_str()))
                return false;
            char* end = nullptr;
            errno = 0;
            unsigned long long const value = std::strtoull(text.c_str(), &end, 10);
            if(errno != 0 || end == text.c_str() || *end != '\0')
                return false;
            out = static_cast<std::uint64_t>(value);
            return true;
        }

        /** Parse an unsigned 32-bit integer without accepting signs or trailing garbage. */
        inline bool parseUint32(std::string const& text, std::uint32_t& out)
        {
            std::uint64_t value = 0;
            if(!parseUint64(text, value) || value > UINT32_MAX)
                return false;
            out = static_cast<std::uint32_t>(value);
            return true;
        }

        /** Parse a signed integer without accepting trailing garbage. */
        inline bool parseInt(std::string const& text, int& out)
        {
            if(text.empty())
                return false;
            char* end = nullptr;
            errno = 0;
            long const value = std::strtol(text.c_str(), &end, 10);
            if(errno != 0 || end == text.c_str() || *end != '\0' || value < INT_MIN || value > INT_MAX)
                return false;
            out = static_cast<int>(value);
            return true;
        }

        inline void printUsage(char const* program)
        {
            std::cout << "Usage: " << program << " [<number of elements> <repeat>] [options]\n"
                      << "  Positional form: " << program << " <number of elements> <repeat>\n"
                      << "  --n N             Number of input elements (default: " << defaultNumElements << ")\n"
                      << "  --chunk N         Elements per block scan, 0 = all (default: " << defaultChunk << ")\n"
                      << "  --repeat R        Inner scans per measured sample (default: " << defaultRepeat << ")\n"
                      << "  --samples S       Independent measured samples (default: " << defaultSamples << ")\n"
                      << "  --type T          Element type: char, short, int, long or all (default: int)\n"
                      << "  --verify-only     Only run the correctness checks, no timing\n"
                      << "  -h, --help        Show this help\n";
        }
    } // namespace detail

    /** Parse the command line.
     *
     * Accepts the HeCBench positional form `<number of elements> <repeat>` and named options. The
     * named options keep the option names, defaults and validation of the original port parser.
     */
    inline int parseCmd(int argc, char* argv[], Params& params)
    {
        params = Params{};

        int positionalCount = 0;
        std::string positional[2];

        for(int i = 1; i < argc; ++i)
        {
            std::string const arg = argv[i];
            if(arg == "-h" || arg == "--help")
            {
                detail::printUsage(argv[0]);
                std::exit(EXIT_SUCCESS);
            }
            if(arg.empty())
            {
                std::cerr << "Error: empty argument.\n";
                return EXIT_FAILURE;
            }
            if(arg[0] != '-')
            {
                if(positionalCount == 2)
                {
                    std::cerr << "Error: too many positional arguments.\n";
                    detail::printUsage(argv[0]);
                    return EXIT_FAILURE;
                }
                positional[positionalCount++] = arg;
                continue;
            }

            std::string name = arg;
            std::string value;
            bool hasValue = false;
            std::size_t const equals = arg.find('=');
            if(equals != std::string::npos)
            {
                name = arg.substr(0, equals);
                value = arg.substr(equals + 1);
                hasValue = true;
            }

            auto requireValue = [&]() -> bool
            {
                if(hasValue)
                    return true;
                if(i + 1 >= argc)
                {
                    std::cerr << "Error: option " << name << " requires a value.\n";
                    return false;
                }
                value = argv[++i];
                return true;
            };

            if(name == "--n")
            {
                if(!requireValue())
                    return EXIT_FAILURE;
                if(!detail::parseUint64(value, params.numElements))
                {
                    std::cerr << "Error: --n must be a non-negative integer.\n";
                    return EXIT_FAILURE;
                }
            }
            else if(name == "--chunk")
            {
                if(!requireValue())
                    return EXIT_FAILURE;
                if(!detail::parseUint32(value, params.chunk))
                {
                    std::cerr << "Error: --chunk must be a non-negative integer.\n";
                    return EXIT_FAILURE;
                }
            }
            else if(name == "--repeat")
            {
                if(!requireValue())
                    return EXIT_FAILURE;
                if(!detail::parseInt(value, params.repeat))
                {
                    std::cerr << "Error: --repeat must be an integer.\n";
                    return EXIT_FAILURE;
                }
            }
            else if(name == "--samples")
            {
                if(!requireValue())
                    return EXIT_FAILURE;
                if(!detail::parseInt(value, params.samples))
                {
                    std::cerr << "Error: --samples must be an integer.\n";
                    return EXIT_FAILURE;
                }
            }
            else if(name == "--type")
            {
                if(!requireValue())
                    return EXIT_FAILURE;
                params.type = value;
            }
            else if(name == "--verify-only")
            {
                if(hasValue)
                {
                    std::cerr << "Error: --verify-only does not take a value.\n";
                    return EXIT_FAILURE;
                }
                params.verifyOnly = true;
            }
            else
            {
                std::cerr << "Error: unknown argument " << arg << ".\n";
                detail::printUsage(argv[0]);
                return EXIT_FAILURE;
            }
        }

        // HeCBench positional form: `<number of elements> <repeat>`.
        if(positionalCount != 0)
        {
            if(positionalCount != 2)
            {
                std::cerr << "Error: positional form requires both <number of elements> and <repeat>.\n";
                detail::printUsage(argv[0]);
                return EXIT_FAILURE;
            }
            if(!detail::parseUint64(positional[0], params.numElements))
            {
                std::cerr << "Error: number of elements must be a non-negative integer.\n";
                return EXIT_FAILURE;
            }
            if(!detail::parseInt(positional[1], params.repeat))
            {
                std::cerr << "Error: repeat must be an integer.\n";
                return EXIT_FAILURE;
            }
        }

        if(params.chunk != 0u && params.chunk != 128u && params.chunk != 256u && params.chunk != 512u
           && params.chunk != 1024u && params.chunk != 2048u)
        {
            std::cerr << "Error: chunk must be one of 0, 128, 256, 512, 1024, 2048.\n";
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
        if(params.type != "char" && params.type != "short" && params.type != "int" && params.type != "long"
           && params.type != "all")
        {
            std::cerr << "Error: type must be one of char, short, int, long, all.\n";
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

} // namespace scanBench
