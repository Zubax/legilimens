/*
 * This is a demo config file for Legilimens. Copy-paste it into your project and adjust as necessary.
 */

#pragma once

namespace legilimens
{
/**
 * Objects of this type are instantiated whenever the library performs atomic operations, such as:
 *  - sampling a variable
 *  - registering/unregistering probe instances at runtime
 */
struct CriticalSectionLocker { };

/**
 * Maximum size of a variable that can be traced.
 * For example, if this number is set to 256, traceable vectors of 32-bit floats may contain up to 256/4 = 64 elements.
 */
constexpr std::size_t MaxVariableSize = 256;

/**
 * How many probes of the same category may exist at the same time.
 * If this number is exceeded in debug builds, an assertion trip will be triggered.
 * If this number is exceeded in release builds, nested variables will be traced incorrectly
 * until the last variable of the given category (the one that overflowed) is removed.
 */
constexpr std::size_t MaxNumberOfCoexistentProbesOfSameCategory = 10;

/**
 * This function is invoked by the library whenever it samples a value, in order to time stamp the sample.
 * The function is ALWAYS invoked from within a critical section, see @ref CriticalSectionLocker.
 * The return type can be arbitrary, as long as it is copyable. The same type will be used by the sampling function.
 * For example, a popular definition is as follows:
 *
 *  std::chrono::nanoseconds getTimeFromCriticalSection();
 *
 * The Legilimens library will take note and make all timestamps typed as std::chrono::nanoseconds.
 */
std::uint64_t getTimeFromCriticalSection();

}
