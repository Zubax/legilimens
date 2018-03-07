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

template <std::size_t Capacity>
using Bytes = senoval::Vector<std::uint8_t, Capacity>;


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

    REQUIRE_FALSE(ProbeName::isValidName(""));
    REQUIRE      (ProbeName::isValidName("0"));
    REQUIRE_FALSE(ProbeName::isValidName("\x80"));
}


/// https://stackoverflow.com/questions/47241504/initialization-of-static-members-of-class-templates-with-side-effects
TEST_CASE("ProbeCategoryRegistration")
{
    REQUIRE      (findProbeCategoryByIndex(0));
    REQUIRE      (findProbeCategoryByIndex(1));
    REQUIRE      (findProbeCategoryByIndex(2));
    REQUIRE      (findProbeCategoryByIndex(3));
    REQUIRE_FALSE(findProbeCategoryByIndex(4));
    REQUIRE_FALSE(findProbeCategoryByIndex(5));

    REQUIRE      (findProbeCategoryByName("a"));
    REQUIRE      (findProbeCategoryByName("b"));
    REQUIRE      (findProbeCategoryByName("check_exists_a"));
    REQUIRE      (findProbeCategoryByName("check_exists_b"));
    REQUIRE_FALSE(findProbeCategoryByName("z"));
    REQUIRE_FALSE(findProbeCategoryByName(""));
    REQUIRE_FALSE(findProbeCategoryByName("\xFF\xA5"));
    REQUIRE_FALSE(findProbeCategoryByName("0123456789012345678901234567890123456789012345678901234567890123456789"
                                              "012345678901234567890123456789012345678901234567890123456789"));
}


TEST_CASE("Probe")
{
    {
        std::int32_t value_a = 0;
        LEGILIMENS_PROBE("a", value_a);

        REQUIRE(findProbeCategoryByName("a")->getName() == "a");
        REQUIRE(findProbeCategoryByName("a")->getTypeDescriptor().number_of_elements == 1);
        REQUIRE(findProbeCategoryByName("a")->getTypeDescriptor().element_size == 4);
        REQUIRE(findProbeCategoryByName("a")->getTypeDescriptor().kind == TypeDescriptor::Kind::Integer);
        REQUIRE(findProbeCategoryByName("a")->sample().size() == 4);
        REQUIRE(findProbeCategoryByName("a")->sample() == Bytes<4>{0, 0, 0, 0});
    }

    REQUIRE(findProbeCategoryByName("a")->getName() == "a");
    REQUIRE(findProbeCategoryByName("a")->getTypeDescriptor().number_of_elements == 1);
    REQUIRE(findProbeCategoryByName("a")->getTypeDescriptor().element_size == 4);
    REQUIRE(findProbeCategoryByName("a")->getTypeDescriptor().kind == TypeDescriptor::Kind::Integer);
    REQUIRE(findProbeCategoryByName("a")->sample().size() == 0);
    REQUIRE(findProbeCategoryByName("a")->sample() == Bytes<4>{});

    {
        std::array<std::uint16_t, 4> value_b{
            0x1234U,
            0x4567U,
            0x89ABU,
            0xCDEFU,
        };

        LEGILIMENS_PROBE("b", value_b);

        REQUIRE(findProbeCategoryByName("b")->getName() == "b");
        REQUIRE(findProbeCategoryByName("b")->getTypeDescriptor().number_of_elements == 4);
        REQUIRE(findProbeCategoryByName("b")->getTypeDescriptor().element_size == 2);
        REQUIRE(findProbeCategoryByName("b")->getTypeDescriptor().kind == TypeDescriptor::Kind::Unsigned);
        REQUIRE(findProbeCategoryByName("b")->sample().size() == 8);
        REQUIRE(findProbeCategoryByName("b")->sample() == Bytes<8>{
            0x34, 0x12,
            0x67, 0x45,
            0xAB, 0x89,
            0xEF, 0xCD,
        });
    }
}


void thisFunctionIsNeverInvoked();
[[maybe_unused]]
void thisFunctionIsNeverInvoked()
{
    (void) &thisFunctionIsNeverInvoked;
    int a = 0;
    LEGILIMENS_PROBE("check_exists_a", a);
}


struct [[maybe_unused]] ThisObjectIsNeverInstantiated
{
    double a = 0;
    LEGILIMENS_PROBE("check_exists_b", a);
};