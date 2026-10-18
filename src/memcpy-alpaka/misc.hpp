/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the memcpy benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 */

#include "helper.hpp"

#include <cerrno>
#include <cctype>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace memcpyBench
{
    //! Memory transfer mode, matching the three directions of the pinned HeCBench reference.
    enum class CopyMode
    {
        hostToDevice,
        deviceToHost,
        deviceToDevice,
        all
    };

    struct Params
    {
        //! Sizes in bytes; empty means the 16 HeCBench default sizes.
        std::vector<std::size_t> sizes;
        int repeat = defaultRepeat;
        int samples = defaultSamples;
        std::size_t maxBytes = defaultMaxBytes;
        CopyMode mode = CopyMode::all;
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
                if(!std::isdigit(static_cast<unsigned char>(*p)))
                    return false;
            return true;
        }

        inline bool parseSizeList(std::string const& text, std::vector<std::size_t>& out)
        {
            std::size_t start = 0;
            while(start <= text.size())
            {
                std::size_t const comma = text.find(',', start);
                std::string const token
                    = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
                if(!allDigits(token.c_str()))
                    return false;
                out.push_back(static_cast<std::size_t>(std::strtoull(token.c_str(), nullptr, 10)));
                if(comma == std::string::npos)
                    break;
                start = comma + 1;
            }
            return !out.empty();
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

        /** Parse an unsigned byte count without accepting signs or trailing garbage. */
        inline bool parseSize(std::string const& text, std::size_t& out)
        {
            if(text.empty() || !allDigits(text.c_str()))
                return false;
            char* end = nullptr;
            errno = 0;
            unsigned long long const value = std::strtoull(text.c_str(), &end, 10);
            if(errno != 0 || end == text.c_str() || *end != '\0')
                return false;
            out = static_cast<std::size_t>(value);
            return true;
        }

        inline void printUsage(char const* program)
        {
            std::cout << "Usage: " << program << " [<repeat>] [options]\n"
                      << "  Positional form: " << program << " <repeat>\n"
                      << "  --sizes A,B,C     Comma separated byte sizes (default: HeCBench sizes)\n"
                      << "  --repeat R        Inner copies per sample (default: " << defaultRepeat << ")\n"
                      << "  --samples S       Independent measured samples (default: " << defaultSamples << ")\n"
                      << "  --max-bytes N     Hard cap for a single allocation in bytes (default: "
                      << defaultMaxBytes << ")\n"
                      << "  --mode M          Transfer mode: h2d, d2h, d2d or all (default: all)\n"
                      << "  --verify-only     Only run the correctness check, no timing\n"
                      << "  -h, --help        Show this help\n";
        }
    } // namespace detail

    /** Parse the command line.
     *
     * Accepts the HeCBench positional form `memcpy <repeat>` and named options. The named options
     * use the same names, defaults and validation as the Catch2 based parser this replaces.
     */
    inline int parseCmd(int argc, char* argv[], Params& params)
    {
        // HeCBench positional form: `./main <repeat>`.
        if(argc == 2 && detail::allDigits(argv[1]))
        {
            params.repeat = std::atoi(argv[1]);
            if(params.repeat <= 0)
            {
                std::cerr << "Error: repeat must be greater than zero.\n";
                return EXIT_FAILURE;
            }
            return EXIT_SUCCESS;
        }

        std::string sizesCsv;
        std::string mode;

        for(int i = 1; i < argc; ++i)
        {
            std::string const arg = argv[i];
            if(arg == "-h" || arg == "--help")
            {
                detail::printUsage(argv[0]);
                std::exit(EXIT_SUCCESS);
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

            if(name == "--sizes")
            {
                if(!requireValue())
                    return EXIT_FAILURE;
                sizesCsv = value;
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
            else if(name == "--max-bytes")
            {
                if(!requireValue())
                    return EXIT_FAILURE;
                if(!detail::parseSize(value, params.maxBytes))
                {
                    std::cerr << "Error: --max-bytes must be a non-negative integer.\n";
                    return EXIT_FAILURE;
                }
            }
            else if(name == "--mode")
            {
                if(!requireValue())
                    return EXIT_FAILURE;
                mode = value;
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
        if(params.maxBytes == 0)
        {
            std::cerr << "Error: max-bytes must be greater than zero.\n";
            return EXIT_FAILURE;
        }

        if(!sizesCsv.empty())
        {
            params.sizes.clear();
            if(!detail::parseSizeList(sizesCsv, params.sizes))
            {
                std::cerr << "Error: --sizes must be a comma separated list of positive integers.\n";
                return EXIT_FAILURE;
            }
        }

        if(!mode.empty())
        {
            if(mode == "h2d")
                params.mode = CopyMode::hostToDevice;
            else if(mode == "d2h")
                params.mode = CopyMode::deviceToHost;
            else if(mode == "d2d")
                params.mode = CopyMode::deviceToDevice;
            else if(mode == "all")
                params.mode = CopyMode::all;
            else
            {
                std::cerr << "Error: --mode must be one of h2d, d2h, d2d, all.\n";
                return EXIT_FAILURE;
            }
        }

        return EXIT_SUCCESS;
    }

    inline char const* modeName(CopyMode mode)
    {
        switch(mode)
        {
        case CopyMode::hostToDevice:
            return "H2D";
        case CopyMode::deviceToHost:
            return "D2H";
        case CopyMode::deviceToDevice:
            return "D2D";
        case CopyMode::all:
            break;
        }
        return "all";
    }
} // namespace memcpyBench
