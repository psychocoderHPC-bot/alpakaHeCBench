/* Copyright 2026 René Widera
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/** @file Command line parsing helpers for the jacobi benchmark.
 * @attention The license differs from the benchmark implementation because this file contains no parts of the
 * HeCBench code base.
 */

#include <cerrno>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace jacobi
{
    //! HeCBench `jacobi` hard codes `#define N 2048`; keep it as the default.
    constexpr int defaultN = 2048;
    //! HeCBench tolerance `1.e-5f`.
    constexpr float defaultTolerance = 1.e-5f;
    //! HeCBench iteration cap `max_iters = 10000`.
    constexpr int defaultMaxIters = 10000;
    //! Number of measured full solves; the HeCBench reference prints one value per invocation.
    constexpr int defaultRepeat = 5;

    //! Benchmark configuration handed to the example function.
    struct Config
    {
        int n = defaultN;
        float tolerance = defaultTolerance;
        int maxIters = defaultMaxIters;
        int repeat = defaultRepeat;
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

        inline bool parseInteger(std::string const& text, int& value)
        {
            if(text.empty())
                return false;
            errno = 0;
            char* end = nullptr;
            long const parsed = std::strtol(text.c_str(), &end, 10);
            if(errno != 0 || end == text.c_str() || *end != '\0')
                return false;
            if(parsed < static_cast<long>(std::numeric_limits<int>::min())
               || parsed > static_cast<long>(std::numeric_limits<int>::max()))
                return false;
            value = static_cast<int>(parsed);
            return true;
        }

        inline bool parseFloat(std::string const& text, float& value)
        {
            if(text.empty())
                return false;
            errno = 0;
            char* end = nullptr;
            float const parsed = std::strtof(text.c_str(), &end);
            if(errno != 0 || end == text.c_str() || *end != '\0' || !std::isfinite(parsed))
                return false;
            value = parsed;
            return true;
        }

        /** Match the supported spelling of a long option and return its value.
         *
         * Accepts `--name value` and `--name=value`. Returns false when @p arg is not
         * this option. A missing value sets @p missing and returns true so the caller
         * can report the exact option.
         */
        inline bool takeOption(
            std::string const& arg,
            char const* name,
            int& index,
            int argc,
            char* argv[],
            std::string& value,
            bool& missing)
        {
            std::string const option = std::string("--") + name;
            if(arg == option)
            {
                if(index + 1 >= argc)
                {
                    missing = true;
                    return true;
                }
                value = argv[++index];
                return true;
            }
            std::string const prefix = option + "=";
            if(arg.compare(0, prefix.size(), prefix) == 0)
            {
                value = arg.substr(prefix.size());
                if(value.empty())
                    missing = true;
                return true;
            }
            return false;
        }
    } // namespace detail

    /** Parse the command line.
     *
     * Accepts a positional form (`<n> [repeat]`) and named options (`--n N --tolerance T --max-iters M
     * --repeat R`). The positional form is checked before any option parsing. Named options accept
     * either a separate value or `--option=value`.
     *
     * @return EXIT_SUCCESS on success, EXIT_FAILURE after printing a diagnostic.
     */
    inline int parseCmd(int argc, char* argv[], Config& cfg)
    {
        cfg = Config{};

        if(argc >= 2 && detail::allDigits(argv[1]))
        {
            cfg.n = std::atoi(argv[1]);
            if(argc >= 3 && detail::allDigits(argv[2]))
                cfg.repeat = std::atoi(argv[2]);
            if(cfg.n < 3 || cfg.repeat <= 0)
            {
                std::cerr << "Error: n must be >= 3 and repeat must be greater than zero.\n";
                return EXIT_FAILURE;
            }
            return EXIT_SUCCESS;
        }

        for(int index = 1; index < argc; ++index)
        {
            std::string const arg = argv[index];
            std::string value;
            bool missing = false;
            bool matched = false;

            matched = detail::takeOption(arg, "n", index, argc, argv, value, missing);
            if(matched)
            {
                if(missing)
                {
                    std::cerr << "Error: --n requires a value.\n";
                    return EXIT_FAILURE;
                }
                if(!detail::parseInteger(value, cfg.n))
                {
                    std::cerr << "Error: --n expects an integer, got '" << value << "'.\n";
                    return EXIT_FAILURE;
                }
                continue;
            }

            matched = detail::takeOption(arg, "tolerance", index, argc, argv, value, missing);
            if(matched)
            {
                if(missing)
                {
                    std::cerr << "Error: --tolerance requires a value.\n";
                    return EXIT_FAILURE;
                }
                if(!detail::parseFloat(value, cfg.tolerance))
                {
                    std::cerr << "Error: --tolerance expects a finite number, got '" << value << "'.\n";
                    return EXIT_FAILURE;
                }
                continue;
            }

            matched = detail::takeOption(arg, "max-iters", index, argc, argv, value, missing);
            if(matched)
            {
                if(missing)
                {
                    std::cerr << "Error: --max-iters requires a value.\n";
                    return EXIT_FAILURE;
                }
                if(!detail::parseInteger(value, cfg.maxIters))
                {
                    std::cerr << "Error: --max-iters expects an integer, got '" << value << "'.\n";
                    return EXIT_FAILURE;
                }
                continue;
            }

            matched = detail::takeOption(arg, "repeat", index, argc, argv, value, missing);
            if(matched)
            {
                if(missing)
                {
                    std::cerr << "Error: --repeat requires a value.\n";
                    return EXIT_FAILURE;
                }
                if(!detail::parseInteger(value, cfg.repeat))
                {
                    std::cerr << "Error: --repeat expects an integer, got '" << value << "'.\n";
                    return EXIT_FAILURE;
                }
                continue;
            }

            std::cerr << "Error: unknown argument '" << arg << "'.\n";
            return EXIT_FAILURE;
        }

        if(cfg.n < 3)
        {
            std::cerr << "Error: n must be >= 3.\n";
            return EXIT_FAILURE;
        }
        if(!(cfg.tolerance > 0.0f))
        {
            std::cerr << "Error: tolerance must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(cfg.maxIters <= 0)
        {
            std::cerr << "Error: max-iters must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        if(cfg.repeat <= 0)
        {
            std::cerr << "Error: repeat must be greater than zero.\n";
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
} // namespace jacobi
