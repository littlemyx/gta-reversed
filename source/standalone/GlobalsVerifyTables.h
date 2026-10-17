#pragma once

// Slice D2 of .notes/DETACH_DATA_PLAN.md: run-time self-check of the string / name tables that were turned into C++ literals. Pointer-bearing tables cannot be compared
// byte by byte against the data image (every pointer differs), so the owning source file registers a check that walks OUR table and the same table in the original
// image (still mapped at its original addresses in this build) and compares what the pointers point at.
//
//     #ifdef NOTSA_TABLE_CHECKS
//     static const ::notsa::globals::TableCheck s_chk{ "CShopping::ms_sectionNames", __FILE__, __LINE__, +[](char* why, size_t n) -> bool { ... return true; } };
//     #endif
//
// `NOTSA_VERIFY_GLOBALS=<file>` (see GlobalsVerify.h) runs all registered checks right after the boot dump and appends one line per check to <file>:
//     # TABLE\t<name>\t<file:line>\tOK|FAIL\t<detail>
// `tools/standalone/verify_globals.py` counts them (a FAIL fails the run). Not part of the PCH: include it only in the files that register a check.
#if defined(NOTSA_GLOBALS_DETACHED) && defined(NOTSA_VERIFY_GLOBALS)
#define NOTSA_TABLE_CHECKS 1
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace notsa::globals {
//! Return true if the table is equal to the image; on a difference write a short description to `why` (at most `whySize` bytes) and return false
using TableCheckFn = bool (*)(char* why, size_t whySize);

struct TableCheck {
    TableCheck(const char* name, const char* file, int line, TableCheckFn fn) noexcept; // GlobalsVerify.cpp
};

//! The pristine data image: `original_data.bin` read from the exe's directory (VA 0x858000 = file offset 0). NOT a mapped image (the run build maps none; the
//! GTASA_IMAGE_DIAG census image is a separate, dev-only mapping). nullptr if the VA is outside the image or the file is missing.
const uint8_t* PristineImage(uint32_t va); // GlobalsVerify.cpp

//! A struct / array of the image at its original address
template<typename T>
inline const T& Image(uint32_t va) {
    return *reinterpret_cast<const T*>(PristineImage(va));
}

//! A pointer FOUND INSIDE the image (it holds an original VA) turned into a pointer to the pristine copy of its target
template<typename T>
inline const T* ImagePtr(const T* originalVa) {
    return originalVa ? reinterpret_cast<const T*>(PristineImage(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(originalVa)))) : nullptr;
}

//! The string an image pointer points at, "(null)" for nullptr (for messages)
inline const char* ImageStr(const char* originalVa) {
    const char* s = ImagePtr(originalVa);
    return s ? s : "(null)";
}

//! Our string against the string the image pointer `originalVa` points at (both null counts as equal)
inline bool SameCStr(const char* ours, const char* originalVa) {
    const char* img = ImagePtr(originalVa);
    return (ours == nullptr || img == nullptr) ? ours == img : std::strcmp(ours, img) == 0;
}
} // namespace notsa::globals
#endif
