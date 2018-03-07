/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Zubax Robotics
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

// We want to ensure that assertion checks are enabled when tests are run, for extra safety
#ifdef NDEBUG
# undef NDEBUG
#endif

// The library should be included first in order to ensure that all necessary headers are included in the library itself
#include <legilimens.hpp>

// Test-only dependencies
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <cmath>

// Note that we should NOT define CATCH_CONFIG_MAIN in the same translation unit which also contains tests;
// that slows things down.
// https://github.com/catchorg/Catch2/blob/master/docs/slow-compiles.md
#include "catch.hpp"


using namespace legilimens;


TEST_CASE("ProbeName")
{
    REQUIRE(ProbeName().isEmpty());
    REQUIRE(ProbeName().toString().empty());

    REQUIRE(!ProbeName("123").isEmpty());
    REQUIRE(ProbeName("123").toString() == "123");

    REQUIRE      (ProbeName() == ProbeName());
    REQUIRE_FALSE(ProbeName() != ProbeName());
    REQUIRE      (ProbeName("123") == ProbeName("123"));
    REQUIRE_FALSE(ProbeName("123") != ProbeName("123"));
    REQUIRE      (ProbeName("123") != ProbeName("123456"));
    REQUIRE_FALSE(ProbeName("123") == ProbeName("123456"));

    /*
     * >>> b = [ord(x) for x in '123456789']
     * >>> out = 0
     * >>> for i, e in enumerate(b):
     * ...     out |= (e << (i * 7))
     * ...
     * >>> out
     * 4139051819874441521
     */
    REQUIRE(ProbeName("123456789").getEncodedChunks()[0] == 4139051819874441521ULL);
    // Zero termination injected after the last byte
    REQUIRE(ProbeName("123456789").getEncodedChunks()[1] == 4066426843206293632ULL);
    REQUIRE(ProbeName("123456789").getEncodedChunks()[2] == 3993801866538139705ULL);
    REQUIRE(ProbeName("123456789").getEncodedChunks()[3] == 3921176889869212856ULL);
}


TEST_CASE("Probes")
{
    {
        std::int32_t value_a = 0;
        LEGILIMENS_PROBE("a", value_a);
    }

    REQUIRE      (findProbeCategoryByIndex(0));
    REQUIRE_FALSE(findProbeCategoryByIndex(1));

    REQUIRE      (findProbeCategoryByName("a"));
    REQUIRE_FALSE(findProbeCategoryByName("b"));
}
