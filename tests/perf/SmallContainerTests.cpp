// SPDX-License-Identifier: MIT
//
// `HOUSE-00029`: implement `util::SmallVector` and `util::FixedString`, **or decide against them
// after measuring**. This file is the measurement. It benchmarks the shapes this codebase actually
// contains rather than a synthetic one, because "a small vector is faster" is true and useless
// without knowing how many of them the frame builds.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Matrix.hpp"

namespace
{
    // Where WhatAShortStdStringCosts' strings publish their storage. Without that escape Apple
    // clang at -O3 elides the long string's new/delete pair (C++14 [expr.new]/10 allows it) and
    // measures 0.25 ns, which is no allocation at all (AM4-302).
    const char* volatile gEscapedStringData = nullptr;

    using Clock = std::chrono::steady_clock;

    /// Median of `kRuns` timings of @p work, in nanoseconds per iteration.
    template<typename F>
    double MedianNanosPerIteration(F&& work, std::size_t iterations)
    {
        constexpr std::size_t kRuns = 9;
        std::vector<double> samples;
        samples.reserve(kRuns);
        for (std::size_t run = 0; run < kRuns; ++run)
        {
            const auto start = Clock::now();
            work(iterations);
            const auto elapsed = std::chrono::duration<double, std::nano>(Clock::now() - start).count();
            samples.push_back(elapsed / static_cast<double>(iterations));
        }
        std::sort(samples.begin(), samples.end());
        return samples[kRuns / 2];
    }

    TEST(SmallContainerTests, WhatASmallHeapVectorActuallyCosts)
    {
        // The shape: a collection of a handful of elements, built and thrown away. `std::vector`
        // heap-allocates; a `SmallVector<T, 8>` would not.
        constexpr std::size_t kIterations = 200000;
        volatile int sink = 0;

        const double heap = MedianNanosPerIteration(
            [&](std::size_t n)
            {
                for (std::size_t i = 0; i < n; ++i)
                {
                    std::vector<int> values;
                    values.reserve(8);
                    for (int v = 0; v < 8; ++v)
                    {
                        values.push_back(v);
                    }
                    sink = static_cast<int>(sink) + values[3];
                }
            },
            kIterations);

        const double stack = MedianNanosPerIteration(
            [&](std::size_t n)
            {
                for (std::size_t i = 0; i < n; ++i)
                {
                    std::array<int, 8> values{};
                    std::size_t count = 0;
                    for (int v = 0; v < 8; ++v)
                    {
                        values[count++] = v;
                    }
                    sink = static_cast<int>(sink) + values[3];
                }
            },
            kIterations);

        std::printf("small collection of 8 ints, built and discarded:\n"
                    "  std::vector (reserve 8)   %7.1f ns\n"
                    "  std::array + count        %7.1f ns\n"
                    "  difference                %7.1f ns  (%.0fx)\n",
                    heap,
                    stack,
                    heap - stack,
                    stack > 0.0 ? heap / stack : 0.0);

        // Not an assertion about which is faster -- that is not in doubt. This asserts only that the
        // benchmark measured SOMETHING, so a compiler that optimised the whole loop away would fail
        // here rather than silently report 0 ns and settle the decision by accident.
        EXPECT_GT(heap, 0.5) << "the heap case optimised away; the measurement is meaningless";
    }

    TEST(SmallContainerTests, WhatTheBonePaletteAllocationCosts)
    {
        // The one place in the code today that builds a container on a per-DRAW path:
        // `MaterialBinder::Bind` for a skinned material fills 72 matrices. 72 x 64 B = 4 608 B, so
        // this is not a "small" vector at all -- inline storage would put 4.6 kB on the stack per
        // call. The alternative is a REUSED buffer, which is what the measurement is for.
        constexpr std::size_t kIterations = 200000;
        const Microsoft::Xna::Framework::Matrix identity =
            Microsoft::Xna::Framework::Matrix::getIdentityProperty();
        volatile float sink = 0.0f;

        const double allocated = MedianNanosPerIteration(
            [&](std::size_t n)
            {
                for (std::size_t i = 0; i < n; ++i)
                {
                    std::vector<Microsoft::Xna::Framework::Matrix> palette(72, identity);
                    sink = sink + palette[3].M11;
                }
            },
            kIterations);

        std::vector<Microsoft::Xna::Framework::Matrix> reused(72, identity);
        const double refilled = MedianNanosPerIteration(
            [&](std::size_t n)
            {
                for (std::size_t i = 0; i < n; ++i)
                {
                    std::fill(reused.begin(), reused.end(), identity);
                    sink = sink + reused[3].M11;
                }
            },
            kIterations);

        std::printf("72-matrix bone palette (4 608 B):\n"
                    "  fresh std::vector per draw  %7.1f ns\n"
                    "  reused buffer, refilled     %7.1f ns\n"
                    "  saving per skinned draw     %7.1f ns\n",
                    allocated,
                    refilled,
                    allocated - refilled);

        EXPECT_GT(allocated, 0.5) << "the allocation optimised away; the measurement is meaningless";
    }

    TEST(SmallContainerTests, WhatAShortStdStringCosts)
    {
        // `util::FixedString`'s case. Short-string optimisation means a std::string under ~15 bytes
        // does not allocate at all on libstdc++, which is most of what this project builds --
        // asset names, category names, log prefixes.
        constexpr std::size_t kIterations = 200000;
        volatile std::size_t sink = 0;

        const double shortString = MedianNanosPerIteration(
            [&](std::size_t n)
            {
                for (std::size_t i = 0; i < n; ++i)
                {
                    std::string name = "L0_KITCHEN";
                    gEscapedStringData = name.data();
                    sink = sink + name.size();
                }
            },
            kIterations);

        const double longString = MedianNanosPerIteration(
            [&](std::size_t n)
            {
                for (std::size_t i = 0; i < n; ++i)
                {
                    std::string name = "Models/Interior/Kitchen/kitchen_worktop_oak_long_variant";
                    gEscapedStringData = name.data();
                    sink = sink + name.size();
                }
            },
            kIterations);

        std::printf("std::string construction:\n"
                    "  10 chars (fits SSO)         %7.1f ns\n"
                    "  57 chars (allocates)        %7.1f ns\n",
                    shortString,
                    longString);

        EXPECT_GT(longString, 0.5) << "the long-string case optimised away";
    }
} // namespace
