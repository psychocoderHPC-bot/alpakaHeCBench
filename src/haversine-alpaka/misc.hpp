/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the HeCBench `haversine` benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 */

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace haversineBench
{
    struct Params
    {
        //! Number of distinct city locations (`num_cities`, HeCBench default 2^21).
        std::uint32_t numCities = defaultNumCities;
        //! Timed repetitions of the distance kernel per measured sample.
        int repeat = 100;
        //! Independent measured samples.
        int samples = 5;
        //! Seed for the deterministic location generator (ignored when @ref input is set).
        std::uint64_t seed = 12345u;
        //! Logical workers per frame (launch-shape hint).
        std::uint32_t frame = 256u;
        //! Optional reference-format `lat lon` file; empty -> generate locations in-process.
        std::string input;
        //! Optional path to write the raw device distance array (validation aid).
        std::string dump;
        //! Run the correctness check only, no timing.
        bool verifyOnly = false;
    };

    namespace detail
    {
        inline bool startsWithDash(char const* s)
        {
            return s != nullptr && s[0] == '-' && s[1] != '\0';
        }

        /** Parse a complete unsigned integer string.
         *
         * The whole token must be consumed and fit the destination type; otherwise the value is
         * left untouched and the caller reports the error.
         */
        template<typename T>
        inline bool parseUnsigned(std::string const& text, T& value)
        {
            if(text.empty())
                return false;
            errno = 0;
            char* end = nullptr;
            unsigned long long const parsed = std::strtoull(text.c_str(), &end, 10);
            if(errno != 0 || end == text.c_str() || *end != '\0')
                return false;
            if(parsed > static_cast<unsigned long long>(static_cast<T>(-1)))
                return false;
            value = static_cast<T>(parsed);
            return true;
        }

        /** Parse a complete signed integer string. */
        template<typename T>
        inline bool parseSigned(std::string const& text, T& value)
        {
            if(text.empty())
                return false;
            errno = 0;
            char* end = nullptr;
            long const parsed = std::strtol(text.c_str(), &end, 10);
            if(errno != 0 || end == text.c_str() || *end != '\0')
                return false;
            if(parsed < static_cast<long>(std::numeric_limits<T>::min())
               || parsed > static_cast<long>(std::numeric_limits<T>::max()))
                return false;
            value = static_cast<T>(parsed);
            return true;
        }
    } // namespace detail

    /** Parse the command line.
     *
     * Accepts the HeCBench positional form `haversine <file> <repeat>` (a reference-format location
     * file plus a repeat count) and the named form below. When a positional file is given it is
     * loaded; otherwise the built-in deterministic generator is used.
     *
     * Named options (each takes one value unless noted):
     *   --cities N --repeat N --samples N --seed N --frame N --input FILE --dump FILE
     *   --verify-only (no value)
     */
    inline int parseCmd(int argc, char* argv[], Params& params)
    {
        bool const positional = (argc == 3) && !detail::startsWithDash(argv[1]);

        if(positional)
        {
            params.input = argv[1];
            params.repeat = std::atoi(argv[2]);
        }
        else
        {
            for(int i = 1; i < argc; ++i)
            {
                std::string const option = argv[i];

                auto nextValue = [&](std::string& out) -> bool
                {
                    if(i + 1 >= argc)
                    {
                        std::cerr << "Error: missing value for " << option << ".\n";
                        return false;
                    }
                    out = argv[++i];
                    return true;
                };

                if(option == "--verify-only")
                {
                    params.verifyOnly = true;
                }
                else
                {
                    std::string value;
                    if(option == "--cities")
                    {
                        if(!nextValue(value))
                            return EXIT_FAILURE;
                        if(!detail::parseUnsigned(value, params.numCities))
                        {
                            std::cerr << "Error: invalid value for --cities: " << value << ".\n";
                            return EXIT_FAILURE;
                        }
                    }
                    else if(option == "--repeat")
                    {
                        if(!nextValue(value))
                            return EXIT_FAILURE;
                        if(!detail::parseSigned(value, params.repeat))
                        {
                            std::cerr << "Error: invalid value for --repeat: " << value << ".\n";
                            return EXIT_FAILURE;
                        }
                    }
                    else if(option == "--samples")
                    {
                        if(!nextValue(value))
                            return EXIT_FAILURE;
                        if(!detail::parseSigned(value, params.samples))
                        {
                            std::cerr << "Error: invalid value for --samples: " << value << ".\n";
                            return EXIT_FAILURE;
                        }
                    }
                    else if(option == "--seed")
                    {
                        if(!nextValue(value))
                            return EXIT_FAILURE;
                        if(!detail::parseUnsigned(value, params.seed))
                        {
                            std::cerr << "Error: invalid value for --seed: " << value << ".\n";
                            return EXIT_FAILURE;
                        }
                    }
                    else if(option == "--frame")
                    {
                        if(!nextValue(value))
                            return EXIT_FAILURE;
                        if(!detail::parseUnsigned(value, params.frame))
                        {
                            std::cerr << "Error: invalid value for --frame: " << value << ".\n";
                            return EXIT_FAILURE;
                        }
                    }
                    else if(option == "--input")
                    {
                        if(!nextValue(params.input))
                            return EXIT_FAILURE;
                    }
                    else if(option == "--dump")
                    {
                        if(!nextValue(params.dump))
                            return EXIT_FAILURE;
                    }
                    else
                    {
                        std::cerr << "Error: unrecognized option " << option << ".\n";
                        return EXIT_FAILURE;
                    }
                }
            }
        }

        if(params.numCities == 0u)
        {
            std::cerr << "Error: --cities must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(params.repeat <= 0)
        {
            std::cerr << "Error: --repeat must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(params.samples <= 0)
        {
            std::cerr << "Error: --samples must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(params.frame == 0u)
        {
            std::cerr << "Error: --frame must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
} // namespace haversineBench
