#pragma once

#include <concepts>
#include <cstdint>
#include <bit>

namespace notsa::detail {
//! The float nearest to 1/N. The bit_cast forces the quotient to be a real 32 bit float: a plain `1.f / N` template argument is kept at higher precision by MSVC
//! (the same extended-precision folding as for an inline `x * (1.f / N)`), which makes `value * Reciprocal` round like a division.
consteval float FixedFloatReciprocal(float n) { return std::bit_cast<float>(std::bit_cast<uint32_t>(1.f / n)); }
}

//! Fixed point number (With implicit conversion to float)
//! Decompression multiplies by `Reciprocal`: the exe does `fild; fmul [float constant]` (the float nearest to 1/CompressValue, e.g. 0x3C010204 for 127)
//! and a division by `CompressValue` gives a different last bit for every non power of two value. Pass the exact exe constant as `Reciprocal` when it
//! is not `1.f / CompressValue` (e.g. 256/2pi: the exe loads 2pi/256 = 0x3CC90FDB). Sites where the exe really uses `fdiv` call `Divide()`.
template<std::integral T, float CompressValue, bool UseRoundingWhenConverting = false, float Reciprocal = notsa::detail::FixedFloatReciprocal(CompressValue)>
class FixedFloat {
public:
    //! Construct as 0
    constexpr FixedFloat() = default;

    //! Construct from an uncompressed value
    constexpr FixedFloat(float v) { Set(v, UseRoundingWhenConverting); }

    //! Construct from a pre-compressed value
    //! Be (very) careful when using this constructor, as mixing up this and the float version will cause bugs...
    // TODO: Maybe use tag-based constructors, because this is kinda bug-prone....
    template<std::integral Y>
    explicit constexpr FixedFloat(Y x) : value(x) {}

    //! Construct from another `FixedFloat` with the same CompressValue
    template<std::integral Y, bool R, float Rcp>
    constexpr FixedFloat(FixedFloat<Y, CompressValue, R, Rcp> x) : value(x.Raw()) {}

    //! Construct from another `FixedFloat` with a different CompressValue
    template<std::integral Y, float CV, bool R, float Rcp>
        requires (CV != CompressValue)
    constexpr FixedFloat(FixedFloat<Y, CV, R, Rcp> x) : FixedFloat{(float)x} {}

    //! Decompress like the exe's `fild; fmul [Reciprocal]`
    constexpr operator float() const { return static_cast<float>(value) * Reciprocal; }

    //! Decompress with a division (only for sites where the exe uses `fdiv`)
    constexpr float Divide() const { return static_cast<float>(value) / CompressValue; }

    //! The raw (compressed) value
    constexpr T Raw() const { return value; }

    //! Set the value (Use this if you want to set using rounding)
    constexpr void Set(float v, bool round = UseRoundingWhenConverting) {
        value = round ? static_cast<T>(v * CompressValue + 0.5f) : static_cast<T>(v * CompressValue);
    }

    // Implementations of (basic) arithmetic ops. Necessary to avoid unnecessary int <=> float conversions
    // (That would otherwise occur when doing arithmetic between 2 FixedFloat instances)
#define IMPLEMENT_FIXED_OPERATOR(_op) \
    template<typename T2, typename CommonT = std::common_type_t<T, T2>> \
    constexpr auto operator _op(FixedFloat<T2, CompressValue, UseRoundingWhenConverting, Reciprocal> o) { \
        return FixedFloat<CommonT, CompressValue, UseRoundingWhenConverting, Reciprocal>{static_cast<CommonT>(this->value) _op static_cast<CommonT>(o.value)}; \
    }

    IMPLEMENT_FIXED_OPERATOR(+);
    IMPLEMENT_FIXED_OPERATOR(-);
    IMPLEMENT_FIXED_OPERATOR(/);
    IMPLEMENT_FIXED_OPERATOR(*);
    
private:
    T value{};
};

// The exe's reciprocal constants (.rdata) for the non power-of-two values used by the game: the float nearest to 1/N, computed here in float
static_assert(std::bit_cast<uint32_t>(notsa::detail::FixedFloatReciprocal(127.f)) == 0x3C010204u); // 0x859BCC
static_assert(std::bit_cast<uint32_t>(notsa::detail::FixedFloatReciprocal(255.f)) == 0x3B808081u); // 0x859A3C
static_assert(std::bit_cast<uint32_t>(notsa::detail::FixedFloatReciprocal(100.f)) == 0x3C23D70Au); // 0x858C58
static_assert(std::bit_cast<uint32_t>(notsa::detail::FixedFloatReciprocal(60.f)) == 0x3C888889u); // 0x859044
static_assert(std::bit_cast<uint32_t>(notsa::detail::FixedFloatReciprocal(20.f)) == 0x3D4CCCCDu); // 0x858C28
static_assert(std::bit_cast<uint32_t>(notsa::detail::FixedFloatReciprocal(32767.f)) == 0x38000100u); // 0x858C7C
static_assert(std::bit_cast<uint32_t>(notsa::detail::FixedFloatReciprocal(16383.5f)) == 0x38800100u); // 0x858EAC
static_assert(std::bit_cast<uint32_t>(notsa::detail::FixedFloatReciprocal(256.f / 6.2831855f)) == 0x3CC90FDBu); // 0x859BBC (Occluder rotations, CoverPoint::Dir: 256 / 2pi)
