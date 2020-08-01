#pragma once

#include <type_traits>

// Inspiration: https://gpfault.net/posts/typesafe-bitmasks.txt.html

// The Enum type should be an enum class (so no implicit conversions)
// with each value having consecutive integer values, starting at zero
// I.e. don't give them values that are already powers of two (you can, but it wouldn't help)

template <typename Enum>
class Bitmask {
    static_assert(std::is_enum_v<Enum>);

    using Underlying = std::underlying_type_t<Enum>;

    static constexpr Underlying toMask(Enum e)
    {
        return 1 << static_cast<Underlying>(e);
    }

public:
    constexpr Bitmask()
    {
    }

    // Not explicit on purpose
    constexpr Bitmask(Enum e)
        : mask_(toMask(e))
    {
    }

    constexpr Bitmask(Underlying maskValue)
        : mask_(maskValue)
    {
    }

    // Some of the functions below don't have an overload for Enum, because the implicit conversion
    // to Bitmask will handle it (and there is no way to implement the overload more nicely).

    constexpr bool test(Enum e) const
    {
        return (mask_ & toMask(e)) > 0;
    }

    constexpr bool test(Bitmask other) const
    {
        return (mask_ & other.mask_) == other.mask_;
    }

    constexpr void set(Bitmask other)
    {
        mask_ |= other.mask_;
    }

    constexpr void toggle(Bitmask other)
    {
        mask_ ^= other.mask_;
    }

    constexpr void clear(Bitmask other)
    {
        mask_ &= ~other.mask_;
    }

    constexpr void clear()
    {
        mask_ = 0;
    }

    constexpr Underlying getMask() const
    {
        return mask_;
    }

    constexpr Bitmask operator|(Bitmask other) const
    {
        return Bitmask(mask_ | other.mask_);
    }

    constexpr Bitmask& operator|=(Bitmask other)
    {
        set(other);
        return *this;
    }

private:
    Underlying mask_ = 0;
};

template <typename T>
constexpr bool isEnumClass
    = std::is_enum_v<T> && !std::is_convertible_v<T, std::underlying_type_t<T>>;

template <typename T>
struct BitmaskEnabled : std::false_type {
};

// You can enable operator| for your enum like this:
// template <> struct BitmaskEnabled<MyEnum> : std::true_type {};

// I could also just use isEnumClass above (std::is_enum_v would be very bad though, because it
// breaks in standard library headers), but I think that's too dangerous. I keep the template there
// for later just in case.

template <class Enum, typename = typename std::enable_if_t<BitmaskEnabled<Enum>::value>>
constexpr Bitmask<Enum> operator|(Enum lhs, Enum rhs)
{
    return Bitmask<Enum>(lhs) | rhs;
}
