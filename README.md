[![Forum](https://img.shields.io/discourse/https/forum.zubax.com/users.svg)](https://forum.zubax.com)
[![Travis CI build status](https://travis-ci.org/Zubax/legilimens.svg?branch=master)](https://travis-ci.org/Zubax/legilimens)

# Legilimens

A very lightweight single-header C++17 library for runtime introspection and tracing in deeply embedded applications.
Legilimens does not introduce any significant overhead and does not interefere with the application when not in use.
It can be used to conduct runtime introspection and tracing of hard real-time systems.

Legilimens has to rely on low-level memory aliasing and heavy compile-time computations to ensure lack
of interference with the application at runtime, but it is easy to use.
In a way, you can think of it as a pico-sized remote debugging server that you can ship with production systems
which lies dormant until needed.

## Usage

The entirety of the library API is just the following simple collection of entities.

Before we describe them, we need to define what a "probe category" is:
a probe category defines a collection of probes that share the same human-readable name
and the same type of the sampled value.

- Macro `LEGILIMENS_PROBE(name, reference)`. Use it to define "probes" - access points for internal states.
Legilimens knows what probes are available in the application statically, by constructing a linked list of them
during static initialization.
The name cannot be longer than 36 characters (the limit may be made configurable in the future).
- Function `Category* findCategoryByName(name)`. Use it to sample probes by name.
- Function `Category* findCategoryByIndex(index)`. Like above, but with index instead of human-readable name.
Indexes are assigned in an arbitrary way during static initialization.
Indexes stay constant as long as the application is running.
This is useful if you want to iterate over all available probe categories.
- Function `std::size_t countCategories()` returns the number of categories registered in the application.
- Function `Name findFirstNonUniqueCategoryName()` returns the name of the first randomly chosen category
which shares its name with another category (i.e., if there are values of different types under the same name).
This is useful if you want to guarantee that the category names are unique,
in which case you should invoke this function shortly after application startup and ensure that it returns an
empty name (meaning that all category names are unique).
- Class `Category` with its methods `Name getName()`, `TypeDescriptor getTypeDescriptor()`, and
`std::pair<Timestamp, SampledBytes> sample()`.
Use it to request metadata about sampled values and perform the actual sampling.

```c++
// Single header only.
#include <legilimens.hpp>

// Tracing a static variable. So easy.
// The tracing entity is called a "probe", and it is created using a macro as shown below.
static float g_static_value = 123.456F;
LEGILIMENS_PROBE("my_static_value",         // <-- human-readable name for this value
                 g_static_value);           // <-- reference to the value

// Tracing a member variable.
// If the class is instantiated multiple times, the probe will point to the least recently instantiated instance.
// Older instances will be unreachable for tracing until the newer ones are removed.
// Think of it as a stack where you can remove items in a random order.
struct MyClass
{
    double a = 0;                           // <-- we're going to trace this
    LEGILIMENS_PROBE("my_class.a",          // <-- human-readable name
                     a);                    // <-- reference to the member variable defined above
};

void foo()
{
    // Local variables can also be traced!
    int local = 0;
    LEGILIMENS_PROBE("foo.local", local);
}

void bar()
{
    // There may be more than one probe under the same name, and they may refer to differently-typed values.
    float local = 0;
    LEGILIMENS_PROBE("foo.local", local);   // <-- this time it's a float
}

void accessExample()
{
    // In a real application don't forget to check for nullptr.
    auto [timestamp, bytes] = legilimens::findCategoryByName("my_class.a")->sample();
    if (bytes.size())
    {
        // Okay, we have sampled the value (atomically!); its image is stored in 'bytes'.
        // Now we can send these bytes to an external system for inspection, logging, plotting, or whatever.
        // The time is sampled atomically with the image.
        send(timestamp, bytes);
    }
    else
    {
        // The value that we attempted to sample did not exist at the moment.
        // For example, its container (if it is a member variable of a class) or its context
        // (if it's a function-local variable) were nonexistent.
    }

    // You can also list all probes that exist in the application statically.
    // The list of probes is always static and never changes while the application is running.
    // For example, the following calls return valid pointers to Category instances,
    // even if their traced values don't exist at the time of calling.
    assert(legilimens::findCategoryByName("my_class.a"));  // Non-null even if there are no instances of MyClass
    assert(legilimens::findCategoryByName("foo.local"));   // Non-null even if foo() is never invoked
    assert(legilimens::findCategoryByIndex(3));            // Non-null because there are >3 probe categories
}
```

Looks convoluted, doesn't it?
The best way to learn how to use it is to just read its source code (and the unit tests).
Luckily, there is not a lot of code -- just a few hundred lines of it.

## Requirements

Legilimens requires a full-featured C++17 compiler with the following standard library headers available:

- `cstdint`
- `cassert`
- `cstring`
- `cstddef`
- `type_traits`
- `tuple`

Legilimens requires Senoval: <https://github.com/Zubax/senoval>, which is a simple header-only dependency-free
C++ utility library for deeply embedded systems. Think of it as a robust replacement of `std::vector`
and stuff that does not use heap, RTTI, or exceptions.

Legilimens **does not use** heap, RTTI, or exceptions, thus being suitable for deeply embedded
high-reliability applications.

Legilimens is time-deterministic and memory-deterministic;
it does not contain variable-complexity routines.

## Development

Use JetBrains CLion or whatever you're into. Use the `test` directory as the project root.

This is how you test: `cd test && cmake . && make && ./legilimens_test`
