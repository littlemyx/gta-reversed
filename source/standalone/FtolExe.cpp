// The exe's CRT converts floats to integers through _ftol2 (0x821B40): x87 `fistp qword` with truncation, the LOW dword is returned. NaN / +-inf -> 0 (the integer
// indefinite 0x8000000000000000), 2^31 <= |x| < 2^63 wraps (low 32 bits of the exact integer). MSVC compiles `(int)f` / `(short)f` to `_ftol2_sse` instead (cvttsd2si:
// 0x80000000 for NaN / out of range) and `(unsigned)f` / `(int64)f` / `(uint8)f` to `_ftol2` (the same x87 sequence as the exe). `_ftol2_sse` is provided here with the
// exe's semantics, so EVERY float -> int32 cast of the game code behaves like the original for NaN / inf / out-of-range values. _ftol2 is provided too so that the CRT's
// ftol2.obj (which defines all three) is not pulled into the link. Verified against the exe's own _ftol2 by review_oracle_test ("_ftol2 override").
#ifdef NOTSA_STANDALONE

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

}

#endif // NOTSA_STANDALONE
