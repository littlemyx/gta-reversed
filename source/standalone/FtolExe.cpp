// The exe's CRT converts floats to integers through _ftol2 (0x821B40): x87 `fistp qword` with truncation, the LOW dword is returned. NaN / +-inf -> 0 (the integer
// indefinite 0x8000000000000000), 2^31 <= |x| < 2^63 wraps (low 32 bits of the exact integer). MSVC compiles `(int)f` / `(short)f` to `_ftol2_sse` instead (cvttsd2si:
// 0x80000000 for NaN / out of range) and `(unsigned)f` / `(int64)f` / `(uint8)f` to `_ftol2` (the same x87 sequence as the exe). `_ftol2_sse` is provided here with the
// exe's semantics, so EVERY float -> int32 cast of the game code behaves like the original for NaN / inf / out-of-range values. _ftol2 is provided too so that the CRT's
// ftol2.obj (which defines all three) is not pulled into the link. Verified against the exe's own _ftol2 by review_oracle_test ("_ftol2 override").
#ifdef NOTSA_STANDALONE

#ifdef NOTSA_PORTABLE
// Portable build (no x87 stack, no naked functions): the same conversions as plain C++ over a double argument (the x87 sequences take st(0)). Nothing in the arm64 / x64 code
// generation calls the CRT's _ftol2 family (clang emits fcvtzs / cvttsd2si, which game code must not use for float -> int: it goes through notsa::fp::Ftol / notsa::detail::Ftol,
// F-5); these are the reference implementations of the exe's semantics, checked against the asm below by tests/portable/f4a_lift_test.cpp (rows ftol2 / ftoul2).
#include "../game_sa/Core/X87Soft.h"
#include <cstdint>
namespace notsa::fp::exe {
// _ftol2: fistp qword with truncation, edx:eax. NaN, +-inf and |x| >= 2^63 give the integer indefinite 0x8000000000000000; everything else the exact truncation.
inline int64_t Ftol2(double x) {
    const double a = x < 0 ? -x : x;
    if (!(a < 9223372036854775808.0)) { return INT64_MIN; }   // also NaN
    return (int64_t)x;
}
inline int32_t  Ftoi2(double x)  { return (int32_t)(uint32_t)(uint64_t)Ftol2(x); }    // low dword
inline uint32_t Ftoui2(double x) { return (uint32_t)(uint64_t)Ftol2(x); }
// _ftoul2: 2^63 <= x (or NaN: fcomip unordered sets CF) => (x - 2^63) truncated, top bit flipped; the subtraction is an x87 fsub at the current precision control (24 in game mode).
inline uint64_t Ftoul2(double x) {
    using namespace notsa::fp::soft;
    if (x == x && x < 9223372036854775808.0) { return (uint64_t)Ftol2(x); }
    const double d = ToDouble(Sub(FromDouble(x), FromFloat(9223372036854775808.0f), 24));
    return (uint64_t)Ftol2(d) ^ 0x8000000000000000ull;
}
} // namespace notsa::fp::exe
#else

extern "C" {

// st(0) -> edx:eax (truncated). Clobbers eax / edx only.
__declspec(naked) long long __cdecl _ftol2() {
    __asm {
        sub esp, 12
        fnstcw word ptr [esp]
        movzx eax, word ptr [esp]
        or eax, 0x0C00
        mov word ptr [esp + 2], ax
        fldcw word ptr [esp + 2]
        fistp qword ptr [esp + 4]
        fldcw word ptr [esp]
        mov eax, dword ptr [esp + 4]
        mov edx, dword ptr [esp + 8]
        add esp, 12
        ret
    }
}

__declspec(naked) int __cdecl _ftol2_sse() {
    __asm { jmp _ftol2 }
}

__declspec(naked) int __cdecl _ftol2_sse_excpt() {
    __asm { jmp _ftol2 }
}

// The remaining public symbols of the CRT member ftol2.obj (dumpbin /symbols libcmtd.lib: __ftol2_sse_excpt __ftol2_sse __ftoi2 __ftoui2 __ftol2 __ftoul2
// __ftoul2_legacy). The linker pulls the whole member as soon as ONE of them is undefined (the game's unsigned / 64 bit casts reference __ftoui2 / __ftoul2),
// so every one of them is defined here. int / uint32 results are the low dword of the truncated x87 conversion (what the exe's _ftol2 returns in eax).
__declspec(naked) int __cdecl _ftoi2() {
    __asm { jmp _ftol2 }
}
__declspec(naked) unsigned int __cdecl _ftoui2() {
    __asm { jmp _ftol2 }
}
// unsigned 64 bit: values >= 2^63 are biased (st0 - 2^63, convert, flip the top bit) like the CRT's version
__declspec(naked) unsigned long long __cdecl _ftoul2() {
    __asm {
        sub esp, 16
        fnstcw word ptr [esp]
        movzx eax, word ptr [esp]
        or eax, 0x0C00
        mov word ptr [esp + 2], ax
        fldcw word ptr [esp + 2]
        mov dword ptr [esp + 12], 0x5F000000 // 2^63
        fld dword ptr [esp + 12]
        fcomip st(0), st(1)                  // CF/ZF: 2^63 <= x
        jbe big
        fistp qword ptr [esp + 4]
        mov eax, dword ptr [esp + 4]
        mov edx, dword ptr [esp + 8]
        jmp done
    big:
        fsub dword ptr [esp + 12]
        fistp qword ptr [esp + 4]
        mov eax, dword ptr [esp + 4]
        mov edx, dword ptr [esp + 8]
        xor edx, 0x80000000
    done:
        fldcw word ptr [esp]
        add esp, 16
        ret
    }
}

// Third-party objects built for IA32 (conan's imgui.lib: ImGui::ImGui*.cpp use float -> unsigned) reference __ftoul2_legacy, which lives in the same CRT
// member (ftol2.obj) as the three above; without a definition here the linker pulls that member in and fails with LNK2005 (_ftol2, _ftol2_sse, _ftol2_sse_excpt).
// Only the game's own objects must follow the exe's semantics, so the truncating x87 sequence is enough (exact for 0 <= x < 2^63).
// (_ftoi2 / _ftoui2 / _ftoul2 above complete the member: all 7 public symbols of ftol2.obj are defined here.)
__declspec(naked) unsigned long long __cdecl _ftoul2_legacy() {
    __asm { jmp _ftol2 }
}

}

#endif // NOTSA_PORTABLE

#endif // NOTSA_STANDALONE
