#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <type_traits>

/*!
 * An all-zero `T`: the value of a global in the exe's data image when the exe's static initialiser leaves it alone (zero .bss, "net write is zero" rows of
 * `.notes/INITTERM_AUDIT.md`) but the repo's default member initialisers / constructors would give it non-zero defaults (`-1` ids, `true` flags, ...).
 * The exe fills such arrays at game init (`Init()`), so the detached build must start from the exe's bytes, not from the repo's defaults.
 * `T` must be default constructible (or trivially copyable) and copyable, and must not have a vptr (use the type's own fix for those, see CAESound).
 */
template<typename T>
inline T ExeZero() {
    static_assert(!std::is_polymorphic_v<T>, "ExeZero<T>: a vptr would be wiped");
    if constexpr (std::is_trivially_copyable_v<T>) {
        return std::bit_cast<T>(std::array<std::byte, sizeof(T)>{});
    } else {
        T o{};
        std::memset(static_cast<void*>(&o), 0, sizeof(T));
        return o;
    }
}

#ifdef NOTSA_GLOBALS_DETACHED
namespace notsa {
/*!
 * True if `p` lies inside this executable's own image (a global / function-local static of the exe, not heap or stack; defined in AESound.cpp).
 * For classes whose exe constructor is empty / leaves the members alone: an object in static storage then keeps its zero .bss bytes
 * (the detached build must match the data image), while objects on the heap or stack still get the repo's defaults.
 */
bool IsInExeImage(const void* p);
}; // namespace notsa
#endif
