#pragma once
// P-2a (.notes/P2_PLAN.md s.2.1): on-disk layout of raw-dumped records, decoupled from the in-memory class.
//
//   notsa::FileRecord<X>   specialise next to X (X.h):   using Type = X_File;  static Type ToFile(const X&);  static void FromFile(X&, const Type&);
//   Absent specialisation  => X is its own file layout (the x86 raw sizeof(X) copy).
//
// The typed CGenericGameStorage::SaveDataToWorkBuffer<T> / LoadDataFromWorkBuffer<T> pick the record path ONLY under NOTSA_PORTABLE; the MSVC build keeps the raw
// sizeof(T) copy (code bytes unchanged). The 64-bit scalar guard below is header-only, compile-time and vacuous when sizeof(void*) == 4.
//
// Standalone header (no StdInc): usable from tests/portable/file_records_test.cpp.
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace notsa {

template<typename T> struct FileRecord; // primary: intentionally undefined

template<typename T>
concept HasFileRecord = requires { typename FileRecord<std::remove_cvref_t<T>>::Type; };

namespace detail {
template<typename T> consteval size_t FileSizeOfImpl() {
    if constexpr (HasFileRecord<T>) { return sizeof(typename FileRecord<std::remove_cvref_t<T>>::Type); }
    else { return sizeof(T); }
}

// Scalars whose width differs between the exe (ILP32) and an LP64 host: pointers, long / unsigned long / size_t / ptrdiff_t / intptr_t / uintptr_t
// (and enums whose underlying type is one of them). Arrays are checked by element type. Vacuous when sizeof(void*) == 4.
template<typename U> consteval bool IsWidthUnsafeScalarImpl() {
    if constexpr (sizeof(void*) <= 4) { return false; }
    else if constexpr (std::is_enum_v<U>) { return IsWidthUnsafeScalarImpl<std::underlying_type_t<U>>(); }
    else if constexpr (std::is_pointer_v<U> || std::is_member_pointer_v<U> || std::is_null_pointer_v<U>) { return true; }
    else if constexpr (sizeof(U) > 4 && std::is_integral_v<U>) {
        return std::is_same_v<U, long> || std::is_same_v<U, unsigned long> || std::is_same_v<U, size_t> || std::is_same_v<U, ptrdiff_t>
            || std::is_same_v<U, intptr_t> || std::is_same_v<U, uintptr_t>;
    } else { return false; }
}
} // namespace detail

//! Bytes of T in a save game / stream: sizeof(FileRecord<T>::Type) if a record exists, else sizeof(T). (Under MSVC x86 both are equal by static_assert.)
template<typename T> inline constexpr size_t FileSizeOf = detail::FileSizeOfImpl<std::remove_cvref_t<T>>();

template<typename T> inline constexpr bool IsWidthUnsafeScalar = detail::IsWidthUnsafeScalarImpl<std::remove_cv_t<std::remove_all_extents_t<std::remove_cvref_t<T>>>>();
template<typename T> inline constexpr bool FileScalarOk = !IsWidthUnsafeScalar<T>;

//! Guard, two layers (both compile-time, both vacuous when sizeof(void*) == 4, i.e. the MSVC x86 build):
//!  1. NOTSA_FILE_SCALAR_GUARD(T): clang `diagnose_if` on the typed Save/Load/RwStreamRead templates => one diagnostic at EVERY call site (file:line of the offender).
//!     Warning by default (the portable build keeps going; the list is the compiler output, see `layout_diff.py guard`), error with NOTSA_FILE_SCALAR_STRICT.
//!  2. `if constexpr (sizeof(void*) > 4) notsa::FileScalarCheck<T>();` in the template body: same rule for compilers without diagnose_if
//!     ([[deprecated]] warning once per instantiation) and a hard static_assert under NOTSA_FILE_SCALAR_STRICT.
//! NOTSA_FILE_SCALAR_STRICT is to be enabled globally after slices b..k (when the offender list is empty).
#define NOTSA_FILE_SCALAR_GUARD_MSG "64-bit-unsafe scalar (pointer/long/size_t/ptrdiff_t/intptr_t) in a save-game/stream read or write: the file layout is 32-bit; use an explicit uint32/int32 (P-2 slice b)"
#if defined(__clang__)
#ifdef NOTSA_FILE_SCALAR_STRICT
#define NOTSA_FILE_SCALAR_GUARD(T) __attribute__((diagnose_if(::notsa::IsWidthUnsafeScalar<T>, NOTSA_FILE_SCALAR_GUARD_MSG, "error")))
#else
#define NOTSA_FILE_SCALAR_GUARD(T) __attribute__((diagnose_if(::notsa::IsWidthUnsafeScalar<T>, NOTSA_FILE_SCALAR_GUARD_MSG, "warning")))
#endif
#else
#define NOTSA_FILE_SCALAR_GUARD(T)
#endif

template<typename T> constexpr void FileScalarCheck() noexcept {}
template<typename T> requires (!FileScalarOk<T>)
#if !defined(NOTSA_FILE_SCALAR_STRICT) && !defined(__clang__)
[[deprecated(NOTSA_FILE_SCALAR_GUARD_MSG)]]
#endif
constexpr void FileScalarCheck() noexcept {
#ifdef NOTSA_FILE_SCALAR_STRICT
    static_assert(FileScalarOk<T>, NOTSA_FILE_SCALAR_GUARD_MSG);
#endif
}

} // namespace notsa
