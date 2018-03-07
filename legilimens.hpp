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
 *
 * Questions and support via https://forum.zubax.com
 */

#ifndef LEGILIMENS_HPP_INCLUDED
#define LEGILIMENS_HPP_INCLUDED

#include <cstdint>
#include <cassert>
#include <cstring>
#include <cstddef>
#include <type_traits>
#include <tuple>

/*
 * User-provided library config header.
 * The following definitions must be provided (all in the "legilimens" namespace):
 *
 *  CriticalSectionLocker - a RAII class that is instantiated when the library performs atomic operations.
 *                          If not required, make it an empty struct, e.g.: struct CriticalSectionLocker { };
 *
 *  constexpr std::size_t MaxVariableSize - maximum size of a traceable variable.
 */
#include <legilimens_config.hpp>

/*
 * A third-party dependency - Senoval - which is a simple header-only library of
 * C++17 classes for real-time embedded systems. The user is expected to make the
 * headers available for inclusion for Legilimens.
 */
#include <senoval/string.hpp>
#include <senoval/vector.hpp>

/**
 * This helper macro allows the user to instantiate an arbitrarily-named probe object with the specified
 * probe name and target variable. It is also possible to instantiate probes manually, should that be necessary.
 *
 * Note that it is absolutely important to keep these superfluous parentheses, because otherwise,
 * in certain contexts, the expression can be interpreted as a FUNCTION DECLARATION rather than
 * VARIABLE DEFINITION (see "most vexing parsing"). I fucking love C++.
 *
 * TODO: remove the hard-coded array indexing and try to apply the solution described here on stackoverflow:
 * https://stackoverflow.com/questions/47263565/expanding-a-constexpr-array-into-a-set-of-non-type-template-parameters
 */
#define LEGILIMENS_PROBE(name, variable)  \
    const ::legilimens::Probe<::legilimens::impl_::CompileTimeTypeDescriptorConstructor<decltype(variable)>, \
                              ::legilimens::ProbeName((name)).getEncodedChunks()[0], \
                              ::legilimens::ProbeName((name)).getEncodedChunks()[1], \
                              ::legilimens::ProbeName((name)).getEncodedChunks()[2], \
                              ::legilimens::ProbeName((name)).getEncodedChunks()[3]> \
        LEGILIMENS_CAT3_(_legilimens_probe_, __LINE__, _) { &(variable) }


#define LEGILIMENS_CAT3_(a, b, c) LEGILIMENS_CAT3_IMPL_(a, b, c)
#define LEGILIMENS_CAT3_IMPL_(a, b, c) a##b##c


namespace legilimens
{
/**
 * Making sure that the user config header provides correct definitions.
 */
static_assert(std::is_default_constructible_v<CriticalSectionLocker>);
static_assert(MaxVariableSize > 0);
static_assert(MaxNumberOfCoexistentProbesOfSameCategory > 0);

/**
 * Short variable name, allows for very fast name matching (virtually a few cycles).
 * It is assumed that each character is a 7-bit ASCII character.
 * Usage of non-ASCII characters may lead to name conflicts and corrupted/unintelligible names.
 *
 * This class converts the supplied string into a set of compile-time integers. The integers can
 * then be used as non-type template parameters. This allows us to transform each unique compile-time
 * string literal into a new type. We use this new type later to create a static list of all known
 * probe names when the runtime is initializing before main().
 */
class ProbeName
{
public:
    typedef std::uint64_t EncodedChunk;

    static constexpr std::size_t NumberOfChunks = 4;

    static constexpr std::size_t CharactersPerChunk = sizeof(EncodedChunk) * 8 / 7;

    static constexpr std::size_t MaxLength = CharactersPerChunk * NumberOfChunks;

private:
    class Chunks
    {
        EncodedChunk chunks_[NumberOfChunks]{};

        template <std::size_t Index>
        bool areChunksEqual(const Chunks& rhs) const
        {
            static_assert(Index > 0, "Logic error (see the specialization)");
            static_assert(Index < NumberOfChunks, "Logic error (index out of range)");

            if (chunks_[Index] == rhs.chunks_[Index]) // Adding UNLIKELY here makes the code slower for some reason
            {
                return areChunksEqual<Index - 1>(rhs);
            }

            return false;
        }

    public:
        constexpr Chunks() { }

        template <typename... EncodedBlockTypes>
        constexpr Chunks(EncodedBlockTypes... encoded_blocks) :
            chunks_{encoded_blocks...}
        {
            static_assert(sizeof...(EncodedBlockTypes) == NumberOfChunks, "Wrong number of arguments");
        }

        constexpr EncodedChunk& operator[](const std::size_t index)
        {
            assert(index < NumberOfChunks);
            return chunks_[index];
        }

        constexpr const EncodedChunk& operator[](const std::size_t index) const
        {
            assert(index < NumberOfChunks);
            return chunks_[index];
        }

        bool operator==(const Chunks& rhs) const
        {
            /*
             * We used to have plain std::equal() here, but you wouldn't believe how slow it was.
             * (approximately 3 times slower)
             * Performance is extremely important here! This function is invoked frequently while
             * scanning the list of variables before tracing is started.
             * Observe also that we compare the strings starting FROM THE END. This approach is
             * statistically optimal: in order for the last block to match false-positively,
             * the two strings should have same exact length and contain same characters at the end.
             */
            return areChunksEqual<NumberOfChunks - 1>(rhs);
        }

        bool operator!=(const Chunks& rhs) const { return !((*this) == rhs); }
    };

    Chunks chunks_;

    static constexpr Chunks encode(const char* const name)
    {
        Chunks b;
        const char* p = name;

        // Simply encoding the string, first characters go into the lowest index chunk
        // And, look, we have a clever trick here.
        // We compare strings starting from the LAST chunk, going towards the FIRST one.
        // This way we can quickly dismiss strings of unequal length and those that have common prefixes.
        // However, comparison of short strings would be slow, because short strings have common (empty) suffixes.
        // Therefore, we fill the chunks with the same string over and over repeatedly!
        // The decoding operation will stop at the first null character anyway, and so it doesn't care what comes next.
        for (std::size_t i = 0; i < MaxLength; i++)
        {
            if (*p != '\0')
            {
                const std::uint8_t ch = std::uint8_t(std::uint32_t(*p) & 0x7FU);
                b[i / CharactersPerChunk] |= EncodedChunk(ch) << (7 * (i % CharactersPerChunk));
                p++;
            }
            else
            {
                // We've reached the end of the string.
                // We're skipping this position in the chunk, making it a null character.
                // Resetting the pointer back to the beginning of the string.
                p = name;
            }
        }

        return b;
    }

public:
    constexpr ProbeName() { }

    constexpr ProbeName(const char* name) :     // Implicit
        chunks_(encode(name))
    { }

    explicit ProbeName(const senoval::String<MaxLength>& name) :
        chunks_(encode(name.c_str()))
    { }

    template <typename... EncodedBlockTypes>
    explicit constexpr ProbeName(EncodedBlockTypes... encoded_blocks) :
        chunks_(std::forward<EncodedBlockTypes>(encoded_blocks)...)
    {
        static_assert(sizeof...(EncodedBlockTypes) == NumberOfChunks, "Wrong number of arguments");
    }

    [[nodiscard]] bool operator==(const ProbeName& rhs) const { return chunks_ == rhs.chunks_; }
    [[nodiscard]] bool operator!=(const ProbeName& rhs) const { return chunks_ != rhs.chunks_; }

    [[nodiscard]] constexpr const Chunks& getEncodedChunks() const { return chunks_; }

    [[nodiscard]] constexpr bool isEmpty() const { return chunks_[0] == 0; }

    [[nodiscard]] senoval::String<MaxLength> toString() const
    {
        senoval::String<MaxLength> ret;
        for (std::size_t i = 0; i < MaxLength; i++)
        {
            const char c = char((chunks_[i / CharactersPerChunk] >> (7 * (i % CharactersPerChunk))) & 0x7FU);
            if (c == '\0')
            {
                break;
            }
            ret.push_back(c);
        }

        return ret;
    }

    /**
     * A helper function that can be used by the application to validate whether the name is valid.
     */
    [[nodiscard]] static constexpr bool isValidName(const char* name)
    {
        if (*name == '\0')
        {
            return false;   // Empty name is not a valid name
        }

        for (std::size_t i = 0; name[i] != '\0'; i++)
        {
            const volatile std::uint32_t x = std::uint32_t(name[i]);
            if (x >= 128)
            {
                return false;
            }
            if (i >= MaxLength)
            {
                return false;
            }
        }

        return true;
    }
};

template <>
inline bool ProbeName::Chunks::areChunksEqual<0>(const Chunks& rhs) const
{
    return chunks_[0] == rhs.chunks_[0];
}

/**
 * Runtime descriptor of the variable type.
 */
struct TypeDescriptor
{
    enum class Kind : std::uint8_t
    {
        Boolean,
        Integer,
        Unsigned,
        Real,
    };

    Kind kind{};
    std::size_t element_size = 0;
    std::size_t number_of_elements = 0;

    constexpr TypeDescriptor() { }

    constexpr TypeDescriptor(Kind arg_kind,
                             std::size_t arg_element_size,
                             std::size_t arg_number_of_elements) :
        kind(arg_kind),
        element_size(arg_element_size),
        number_of_elements(arg_number_of_elements)
    { }

    constexpr bool operator!=(const TypeDescriptor& rhs) const { return !this->operator==(rhs); }
    constexpr bool operator==(const TypeDescriptor& rhs) const
    {
        return (kind                == rhs.kind) &&
               (element_size        == rhs.element_size) &&
               (number_of_elements  == rhs.number_of_elements);
    }

    template <typename ScalarType>
    [[nodiscard]] constexpr static Kind deduceKind()
    {
        using T = std::decay_t<ScalarType>;
        static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);

        if constexpr (std::is_same_v<T, bool>)
        {
            return Kind::Boolean;
        }
        else if constexpr (std::is_integral_v<T>)
        {
            if constexpr (std::is_signed_v<T>)
            {
                return Kind::Integer;
            }
            else
            {
                return Kind::Unsigned;
            }
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            return Kind::Real;
        }
        else
        {
            assert(false);
            return {};
        }
    }
};

/**
 * Implementation details, for use only within the library itself!
 */
namespace impl_
{
/**
 * From the standpoint of the type system of the language, the following types are entirely different:
 *  float[4]
 *  std::array<float, 4>
 *  Eigen::Matrix<float, 2, 2>
 *  Eigen::Matrix<float, 4, 1>
 * However, for the purposes of our library, they are equivalent. Therefore, we reduce the type information
 * to the simple form defined here, in order to correctly represent their equivalency.
 */
template <TypeDescriptor::Kind ElementKind,
          std::size_t ElementSize,
          std::size_t NumberOfElements>
struct CompileTimeTypeDescriptor
{
    static_assert(ElementSize > 0, "Element size must be positive");
    static_assert(NumberOfElements > 0, "Number of elements must be positive");
    static_assert((ElementSize * NumberOfElements) <= MaxVariableSize, "The type is too large to be traceable");

    /**
     * Discards the compile-time type information and returns a runtime type description object.
     */
    constexpr static TypeDescriptor getRuntimeTypeDescriptor()
    {
        return {
            ElementKind,
            ElementSize,
            NumberOfElements
        };
    }
};

template <typename Container>
using ContainerElementType = std::decay_t<decltype(*std::declval<Container>().data())>;

template <typename Container>
constexpr std::size_t ContainerElementSize = sizeof(ContainerElementType<Container>);

/// Overload for std::array<> and similar containers that provide std::tuple_size<>.
template <typename Container>
static constexpr std::enable_if_t<(std::tuple_size<Container>::value > 0), std::size_t> getContainerSize()
{
    return std::tuple_size_v<Container>;
}

/// Overload for Eigen::Matrix and similar containers that provide a static constant SizeAtCompileTime.
template <typename Container>
static constexpr std::enable_if_t<(Container::SizeAtCompileTime > 0), std::size_t> getContainerSize()
{
    return Container::SizeAtCompileTime;
}


template <typename T>
static constexpr auto constructCompileTimeTypeDescriptor()
{
    if constexpr (std::is_integral_v<std::decay_t<T>> || std::is_floating_point_v<std::decay_t<T>>)
    {
        return CompileTimeTypeDescriptor<TypeDescriptor::deduceKind<T>(), sizeof(std::decay_t<T>), 1>();
    }
    else
    {
        return CompileTimeTypeDescriptor<TypeDescriptor::deduceKind<ContainerElementType<T>>(),
                                         ContainerElementSize<T>,
                                         getContainerSize<T>()>();
    }
}

/**
 * Compile-time constructor of type descriptor - @ref CompileTimeTypeDescriptor<>.
 * Generates a compile-time failure if the type cannot be traced.
 */
template <typename T>
using CompileTimeTypeDescriptorConstructor = decltype(constructCompileTimeTypeDescriptor<T>());

/**
 * A bunch of static_assert<> checks that make sure that the compile-time type deduction machinery is working properly.
 */
namespace compile_time_tests_
{

struct LikeEigenMatrix
{
    const float* data() const { return nullptr; }
    enum { SizeAtCompileTime = 42 };   // The way it is used in Eigen
};

static_assert(ContainerElementSize<std::array<std::int64_t, 1000>> == 8);
static_assert(ContainerElementSize<std::array<std::uint16_t, 100>> == 2);
static_assert(ContainerElementSize<std::array<std::uint8_t, 10>> == 1);

static_assert(getContainerSize<std::array<std::int64_t, 1000>>() == 1000);
static_assert(getContainerSize<std::array<std::uint16_t, 100>>() == 100);
static_assert(getContainerSize<std::array<std::uint8_t, 10>>() == 10);

static_assert(getContainerSize<LikeEigenMatrix>() == 42);

static_assert(CompileTimeTypeDescriptorConstructor<bool>::getRuntimeTypeDescriptor().element_size == sizeof(bool));
static_assert(CompileTimeTypeDescriptorConstructor<bool>::getRuntimeTypeDescriptor().number_of_elements == 1);
static_assert(CompileTimeTypeDescriptorConstructor<bool>::getRuntimeTypeDescriptor().kind ==
              TypeDescriptor::Kind::Boolean);

static_assert(CompileTimeTypeDescriptorConstructor<std::uint64_t>::getRuntimeTypeDescriptor().element_size == 8);
static_assert(CompileTimeTypeDescriptorConstructor<std::uint64_t>::getRuntimeTypeDescriptor().number_of_elements == 1);
static_assert(CompileTimeTypeDescriptorConstructor<std::uint64_t>::getRuntimeTypeDescriptor().kind ==
              TypeDescriptor::Kind::Unsigned);

static_assert(CompileTimeTypeDescriptorConstructor<const volatile std::int32_t&>::getRuntimeTypeDescriptor()
                  .element_size == 4);
static_assert(CompileTimeTypeDescriptorConstructor<const volatile std::int32_t&>::getRuntimeTypeDescriptor()
                  .number_of_elements == 1);
static_assert(CompileTimeTypeDescriptorConstructor<const volatile std::int32_t&>::getRuntimeTypeDescriptor()
                  .kind == TypeDescriptor::Kind::Integer);

static_assert(CompileTimeTypeDescriptorConstructor<float&>::getRuntimeTypeDescriptor().element_size == sizeof(float));
static_assert(CompileTimeTypeDescriptorConstructor<float&>::getRuntimeTypeDescriptor().number_of_elements == 1);
static_assert(CompileTimeTypeDescriptorConstructor<float&>::getRuntimeTypeDescriptor().kind ==
              TypeDescriptor::Kind::Real);

static_assert(CompileTimeTypeDescriptorConstructor<std::array<std::int16_t, 3>>::getRuntimeTypeDescriptor()
                  .element_size == 2);
static_assert(CompileTimeTypeDescriptorConstructor<std::array<std::int16_t, 3>>::getRuntimeTypeDescriptor()
                  .number_of_elements == 3);
static_assert(CompileTimeTypeDescriptorConstructor<std::array<std::int16_t, 3>>::getRuntimeTypeDescriptor()
                  .kind == TypeDescriptor::Kind::Integer);

static_assert(CompileTimeTypeDescriptorConstructor<LikeEigenMatrix>::getRuntimeTypeDescriptor()
                  .element_size == sizeof(float));
static_assert(CompileTimeTypeDescriptorConstructor<LikeEigenMatrix>::getRuntimeTypeDescriptor()
                  .number_of_elements == 42);
static_assert(CompileTimeTypeDescriptorConstructor<LikeEigenMatrix>::getRuntimeTypeDescriptor()
                  .kind == TypeDescriptor::Kind::Real);

} // namespace compile_time_tests_

/**
 * Extremely fast memory copy algorithm. For short data blocks it is about twice faster than std::memcpy() on Cortex-M4.
 * The function performs word-sized copies if size, source, and destination are all properly aligned.
 * Otherwise, plain byte-by-byte copying is performed.
 * @param size  Amount of bytes to copy. This value cannot be zero, otherwise the behavior is undefined!
 * @param src   Where to copy from.
 * @param dst   Where to copy to.
 * @return      The new value of dst.
 */
static inline void copyBytesQuicklyAndUnsafely(std::size_t size,
                                               const volatile std::uint8_t* src,
                                               std::uint8_t* dst)
{
    using std::size_t;
    static constexpr unsigned WordSize = sizeof(size_t);

    if ((size                          % WordSize == 0) &&
        (reinterpret_cast<size_t>(dst) % WordSize == 0) &&
        (reinterpret_cast<size_t>(src) % WordSize == 0))
    {
        // Fast copy, one native word per iteration
        do
        {
            *reinterpret_cast<size_t*>(dst) = *reinterpret_cast<const volatile size_t*>(src);
            dst += WordSize;
            src += WordSize;
            size -= WordSize;
        }
        while (size > 0);
    }
    else
    {
        // Slow copy, one byte per iteration
        do
        {
            *dst = *src;
            ++dst;
            ++src;
        }
        while (bool(--size));
    }
}

} // namespace impl_

/**
 * A runtime (i.e. non statically typed) descriptor of a probe.
 * There is one instance of ProbeCategory per probe name and type.
 * Objects of this type are non-copyable (because they are linked-listed).
 * All existing objects are only constructed before main() and exist until the program is terminated.
 * All existing objects are collected in the same linked list.
 * This Stack Overflow question is highly relevant: https://stackoverflow.com/questions/47241504/
 */
class ProbeCategory
{
    ProbeCategory* next_instance_in_list_;

    const TypeDescriptor type_descriptor_;
    const ProbeName name_;
    senoval::Vector<const volatile void*, MaxNumberOfCoexistentProbesOfSameCategory> live_variable_stack_{};
    const volatile void* live_variable_stack_top_ = nullptr;

    static ProbeCategory*& getMutableListRoot()
    {
        static ProbeCategory* root = nullptr;
        return root;
    }

protected:
    ProbeCategory(const TypeDescriptor& arg_type_descriptor,
                  const ProbeName& arg_name) :
        next_instance_in_list_(getMutableListRoot()),
        type_descriptor_(arg_type_descriptor),
        name_(arg_name)
    {
        getMutableListRoot() = this;
    }

    /**
     * Normally, the destructor should be invoked only at the very end of the program,
     * where we could just silently die and nobody would care. However, we still enforce correctness
     * of the linked list for two reasons:
     *  - general consistency;
     *  - avoiding corruption of the list if the user ever decides to create an instance of this class themselves.
     */
    ~ProbeCategory()
    {
        if (getMutableListRoot() == this)
        {
            getMutableListRoot() = this->next_instance_in_list_;
        }
        else
        {
            ProbeCategory* item = getMutableListRoot();
            while (item != nullptr)
            {
                if (item->next_instance_in_list_ == this)
                {
                    item->next_instance_in_list_ = item->next_instance_in_list_->next_instance_in_list_;
                    break;
                }
            }
            assert(item != nullptr);  // nullptr means that we traversed until the end and didn't find the object, bug
        }
    }

    void pushVariable(const volatile void* location)
    {
        assert(location != nullptr);

        CriticalSectionLocker locker;
        (void) locker;

        live_variable_stack_.push_back(location);
        live_variable_stack_top_ = location;
    }

    void popVariable()
    {
        CriticalSectionLocker locker;
        (void) locker;

        live_variable_stack_.pop_back();
        if (!live_variable_stack_.empty())
        {
            live_variable_stack_top_ = live_variable_stack_.back();
        }
        else
        {
            live_variable_stack_top_ = nullptr;
        }
    }

public:
    /**
     * Used for accessing and traversing the global linked list of probes. Normally, the user should not use
     * these methods; instead, use the accessor functions defined at the namespace scope.
     */
    [[nodiscard]] static const ProbeCategory* getListRoot() { return getMutableListRoot(); }
    [[nodiscard]] const ProbeCategory* getNextInstance() const { return next_instance_in_list_; }

    /**
     * Name of the probe category, i.e. name of all probes belonging to this category.
     */
    [[nodiscard]] const ProbeName& getName() const { return name_; }

    /**
     * Type of the variable pointed to by this probe category, i.e. type of variables of all probes
     * belonging to this category.
     */
    [[nodiscard]] const TypeDescriptor& getTypeDescriptor() const { return type_descriptor_; }

    /**
     * Collects the data from the variable and returns it as an array of bytes.
     * Returns an empty array if no such variable exists at this moment.
     */
    [[nodiscard]] senoval::Vector<std::uint8_t, MaxVariableSize> sample() const
    {
        const std::size_t data_size = type_descriptor_.element_size * type_descriptor_.number_of_elements;
        assert(data_size <= MaxVariableSize);       // The size constraint is imposed at compile time also
        senoval::Vector<std::uint8_t, MaxVariableSize> out(data_size, 0);
        bool success = false;

        // The critical section must be as short as possible!
        {
            CriticalSectionLocker locker;
            (void) locker;
            if (live_variable_stack_top_ != nullptr)
            {
                impl_::copyBytesQuicklyAndUnsafely(data_size,
                                                   static_cast<const volatile std::uint8_t*>(live_variable_stack_top_),
                                                   out.data());
                success = true;
            }
        }

        if (!success)
        {
            out.clear();    // It is important to move this out of the critical section
        }

        return out;
    }

    // This class is non-copyable because its objects are members of a static linked list.
    ProbeCategory(const ProbeCategory&) = delete;
    ProbeCategory& operator=(const ProbeCategory&) = delete;
};

/**
 * The probe class that is instantiated by the application whenever there is something to trace.
 * It is quite cumbersome to construct manually; consider using the macro @ref LEGILIMENS_PROBE() instead.
 * Each differently named probe must be a distinct type, hence we keep the list of encoded name chunks in the
 * list of template parameters. This allows us to force the C++ runtime to construct a linked list of all
 * known probe types before main() is invoked. See https://stackoverflow.com/questions/47241504/
 *
 * This class is non-copyable because if it is relocated, that implies that the variable it is tracing might end
 * up relocated also, which would invalidate the stored pointer. Therefore we simply declare it non-copyable;
 * if necessary, the application may opt to use std::optional<> to enable copyability manually (simply destroy and
 * re-create probes whenever your object is moved).
 */
template <typename CompileTimeTypeDescriptor, ProbeName::EncodedChunk... EncodedNameChunks>
class Probe final
{
    static_assert(sizeof...(EncodedNameChunks) == ProbeName::NumberOfChunks,
                  "The list of encoded name blocks is invalid.");

    struct PublicMorozov final : public ProbeCategory
    {
        PublicMorozov() :
            ProbeCategory(CompileTimeTypeDescriptor::getRuntimeTypeDescriptor(),
                          ProbeName(EncodedNameChunks...))
        { }

        using ProbeCategory::pushVariable;
        using ProbeCategory::popVariable;
    };

    static PublicMorozov this_category_;

public:
    /**
     * Constructor for primitive scalar types.
     */
    template <typename T>
    explicit Probe(const T* value,
                   std::enable_if_t<std::is_integral_v<T> || std::is_floating_point_v<T>, int> = 0)
    {
        this_category_.pushVariable(static_cast<const volatile void*>(value));
    }

    /**
     * Constructor for containers that provide .data() const.
     */
    template <typename C, typename E = impl_::ContainerElementType<C>>
    explicit Probe(const C* cont,
                   std::enable_if_t<!(std::is_integral_v<C> || std::is_floating_point_v<C>), int> = 0)
    {
        this_category_.pushVariable(static_cast<const volatile void*>(cont->data()));
    }

    ~Probe()
    {
        this_category_.popVariable();
    }

    // This class is non-copyable, see above
    Probe(const Probe&) = delete;
    Probe& operator=(const Probe&) = delete;
};

template <typename CompileTimeTypeDescriptor, ProbeName::EncodedChunk... EncodedNameChunks>
typename Probe<CompileTimeTypeDescriptor, EncodedNameChunks...>::PublicMorozov
    Probe<CompileTimeTypeDescriptor, EncodedNameChunks...>::this_category_;

/**
 * All ProbeCategory instances are ordered in an arbitrary but stable way; ordering is constant as long as the
 * program is running. This function traverses the list and returns a pointer to the probe object at the specified
 * index. If the index is out of range, a null pointer is returned.
 */
[[nodiscard]]
inline const ProbeCategory* findProbeCategoryByIndex(std::size_t index)
{
    const ProbeCategory* item = ProbeCategory::getListRoot();
    while ((item != nullptr) && (index --> 0))
    {
        item = item->getNextInstance();
    }
    return item;
}

/**
 * Searches the list of ProbeCategory objects by name.
 * Returns the first probe category with a matching name. If there is more than one category under that name
 * (that is possible if they have different types), only the first one can be accessed using this function.
 * Returns null pointer if there is no ProbeCategory under this name.
 */
[[nodiscard]]
inline const ProbeCategory* findProbeCategoryByName(const ProbeName& name)
{
    const ProbeCategory* item = ProbeCategory::getListRoot();
    while (item != nullptr)
    {
        if (item->getName() == name)
        {
            return item;
        }
        item = item->getNextInstance();
    }
    assert(item == nullptr);
    return nullptr;
}

/**
 * Number of probe category objects registered in the application.
 * This function traverses the entire linked list at every invocation.
 */
[[nodiscard]]
inline std::size_t countProbeCategories()
{
    std::size_t out = 0;
    const ProbeCategory* item = ProbeCategory::getListRoot();
    while (item != nullptr)
    {
        ++out;
        item = item->getNextInstance();
    }
    return out;
}

/**
 * This function traverses the list of probe categories and checks that for every existing name there is only
 * one matching probe category. In other words, it ensures that there are no similarly named probe categories
 * that point to variables of different types.
 * Returns an empty string if check has passed; otherwise, returns the first conflicting name by value.
 * Normally one may want to use it in debug builds, if it is important to ensure uniqueness of names:
 *      assert(legilimens::findFirstNonUniqueProbeCategoryName().isEmpty());
 * If you don't care about uniqueness, don't use this function.
 * Beware that the complexity is quadratic of the number of probe categories! This operation is very slow.
 */
[[nodiscard]]
inline ProbeName findFirstNonUniqueProbeCategoryName()
{
    const ProbeCategory* outer = ProbeCategory::getListRoot();
    while (outer != nullptr)
    {
        const ProbeCategory* inner = ProbeCategory::getListRoot();
        while (inner != nullptr)
        {
            if ((inner != outer) &&
                (inner->getName() == outer->getName()))
            {
                return inner->getName();
            }
            inner = inner->getNextInstance();
        }
        assert(inner == nullptr);
        outer = outer->getNextInstance();
    }
    assert(outer == nullptr);
    return {};
}

}   // namespace legilimens

#endif // LEGILIMENS_HPP_INCLUDED
