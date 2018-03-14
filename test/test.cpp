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


namespace legilimens
{

static std::uint64_t g_mock_time = 0;

std::uint64_t getTimeFromCriticalSection()
{
    return g_mock_time;
}

}

using namespace legilimens;

template <std::size_t Capacity>
using Bytes = senoval::Vector<std::uint8_t, Capacity>;

namespace
{
/*
 * Checking static initialization.
 * If the Category constructor is invoked after the probe is initialized, we'll get an assertion failure at
 * the exit because it would appear as if the probe attempted to pop the variable stack while it's empty.
 */
float g_static_value = 123.456F;
LEGILIMENS_PROBE("static", g_static_value);
}


TEST_CASE("Name")
{
    REQUIRE(Name().isEmpty());
    REQUIRE(Name().toString().empty());

    REQUIRE(!Name("123").isEmpty());
    REQUIRE(Name("123").toString() == "123");

    REQUIRE      (Name() == Name());
    REQUIRE_FALSE(Name() != Name());
    REQUIRE      (Name("123") == Name("123"));
    REQUIRE_FALSE(Name("123") != Name("123"));
    REQUIRE      (Name("123") != Name("123456"));
    REQUIRE_FALSE(Name("123") == Name("123456"));

    /*
     * >>> b = [ord(x) for x in '123456789']
     * >>> out = 0
     * >>> for i, e in enumerate(b):
     * ...     out |= (e << (i * 7))
     * ...
     * >>> out
     * 4139051819874441521
     */
    REQUIRE(Name("123456789").getEncodedChunks()[0] == 4139051819874441521ULL);
    // Zero termination injected after the last byte
    REQUIRE(Name("123456789").getEncodedChunks()[1] == 4066426843206293632ULL);
    REQUIRE(Name("123456789").getEncodedChunks()[2] == 3993801866538139705ULL);
    REQUIRE(Name("123456789").getEncodedChunks()[3] == 3921176889869212856ULL);

    REQUIRE_FALSE(Name::isValidName(""));
    REQUIRE      (Name::isValidName("0"));
    REQUIRE_FALSE(Name::isValidName("\x80"));
}


/// https://stackoverflow.com/questions/47241504/initialization-of-static-members-of-class-templates-with-side-effects
TEST_CASE("CategoryRegistration")
{
    std::cout << "findFirstNonUniqueCategoryName(): "
              << findFirstNonUniqueCategoryName().toString().c_str()
              << std::endl;
    REQUIRE(findFirstNonUniqueCategoryName().isEmpty());

    REQUIRE(countCategories() == 6);

    REQUIRE      (findCategoryByIndex(0));
    REQUIRE      (findCategoryByIndex(1));
    REQUIRE      (findCategoryByIndex(2));
    REQUIRE      (findCategoryByIndex(3));
    REQUIRE      (findCategoryByIndex(4));
    REQUIRE      (findCategoryByIndex(5));
    REQUIRE_FALSE(findCategoryByIndex(6));
    REQUIRE_FALSE(findCategoryByIndex(7));

    REQUIRE      (findCategoryByName("a"));
    REQUIRE      (findCategoryByName("b"));
    REQUIRE      (findCategoryByName("check_exists_a"));
    REQUIRE      (findCategoryByName("check_exists_b"));
    REQUIRE      (findCategoryByName("static"));
    REQUIRE_FALSE(findCategoryByName("z"));
    REQUIRE_FALSE(findCategoryByName(""));
    REQUIRE_FALSE(findCategoryByName("\xFF\xA5"));
    REQUIRE_FALSE(findCategoryByName("0123456789012345678901234567890123456789012345678901234567890123456789"
                                         "012345678901234567890123456789012345678901234567890123456789"));

    {
        struct PublicMorozov : public Category
        {
            PublicMorozov() : Category(TypeDescriptor(), "conflicting") {}
        };

        REQUIRE(findFirstNonUniqueCategoryName().isEmpty());
        PublicMorozov conflicting_a;

        REQUIRE(countCategories() == 7);

        {
            PublicMorozov conflicting_c;    // This is needed to test linked list removal
            REQUIRE(countCategories() == 8);
            PublicMorozov conflicting_d;
            REQUIRE(countCategories() == 9);
        }

        REQUIRE(countCategories() == 7);
        PublicMorozov conflicting_b;
        REQUIRE(countCategories() == 8);
        REQUIRE(findFirstNonUniqueCategoryName() == "conflicting");
    }

    REQUIRE(countCategories() == 6);
}


TEST_CASE("Probe")
{
    g_mock_time = 123456;

    {
        std::int32_t value_a = 0;
        LEGILIMENS_PROBE("a", value_a);

        REQUIRE(findCategoryByName("a")->getName() == "a");
        REQUIRE(findCategoryByName("a")->getTypeDescriptor().number_of_elements == 1);
        REQUIRE(findCategoryByName("a")->getTypeDescriptor().element_size == 4);
        REQUIRE(findCategoryByName("a")->getTypeDescriptor().kind == TypeDescriptor::Kind::Integer);
        REQUIRE(findCategoryByName("a")->sample().first == g_mock_time);
        REQUIRE(findCategoryByName("a")->sample().second.size() == 4);
        REQUIRE(findCategoryByName("a")->sample().second == Bytes<4>{0, 0, 0, 0});
    }

    g_mock_time = 654321;

    REQUIRE(findCategoryByName("a")->getName() == "a");
    REQUIRE(findCategoryByName("a")->getTypeDescriptor().number_of_elements == 1);
    REQUIRE(findCategoryByName("a")->getTypeDescriptor().element_size == 4);
    REQUIRE(findCategoryByName("a")->getTypeDescriptor().kind == TypeDescriptor::Kind::Integer);
    REQUIRE(findCategoryByName("a")->sample().first == g_mock_time);
    REQUIRE(findCategoryByName("a")->sample().second.size() == 0);
    REQUIRE(findCategoryByName("a")->sample().second == Bytes<4>{});

    g_mock_time = 987123;

    {
        std::array<std::uint16_t, 4> value_b{{
            0x1234U,
            0x4567U,
            0x89ABU,
            0xCDEFU,
        }};

        LEGILIMENS_PROBE("b", value_b);

        REQUIRE(findCategoryByName("b")->getName() == "b");
        REQUIRE(findCategoryByName("b")->getTypeDescriptor().number_of_elements == 4);
        REQUIRE(findCategoryByName("b")->getTypeDescriptor().element_size == 2);
        REQUIRE(findCategoryByName("b")->getTypeDescriptor().kind == TypeDescriptor::Kind::Unsigned);
        REQUIRE(findCategoryByName("b")->sample().first == g_mock_time);
        REQUIRE(findCategoryByName("b")->sample().second.size() == 8);
        REQUIRE(findCategoryByName("b")->sample().second == Bytes<8>{{
            0x34, 0x12,
            0x67, 0x45,
            0xAB, 0x89,
            0xEF, 0xCD,
        }});
    }

    g_mock_time = 321789;

    {
        struct EigenLike
        {
            std::uint8_t storage[4]{};
            const std::uint8_t* data() const { return &storage[0]; }
            std::uint8_t& operator[](std::size_t index) { return storage[index]; }
            enum { SizeAtCompileTime = sizeof(storage) / sizeof(storage[0]) };
        };

        EigenLike value_c;
        value_c[0] = 1;
        value_c[1] = 2;
        value_c[2] = 3;
        value_c[3] = 4;

        auto getter = [&]() -> const EigenLike& { return value_c; };

        LEGILIMENS_PROBE("c", getter());

        REQUIRE(findCategoryByName("c")->getName() == "c");
        REQUIRE(findCategoryByName("c")->getTypeDescriptor().number_of_elements == 4);
        REQUIRE(findCategoryByName("c")->getTypeDescriptor().element_size == 1);
        REQUIRE(findCategoryByName("c")->getTypeDescriptor().kind == TypeDescriptor::Kind::Unsigned);
        REQUIRE(findCategoryByName("c")->sample().first == g_mock_time);
        REQUIRE(findCategoryByName("c")->sample().second.size() == 4);
        REQUIRE(findCategoryByName("c")->sample().second == Bytes<8>{{
            1, 2, 3, 4,
        }});
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
