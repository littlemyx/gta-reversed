// P2E exe-oracle for GAME functions: runs the original machine code of gta_sa_compact.exe in the test process, next to the C++ port.
//
// How it works: the test exe is linked with tools/standalone/make_orig_pad.py's placeholder (a 9 MB `.text` object first on the link line,
// /BASE:0x400000) so the process owns [0x401000, 0xCB0000) at the exe's ORIGINAL addresses. Map() copies every section of the exe over it
// (read at test time from env RW_EXE_ORACLE; no exe bytes are stored in the repo), makes the range RWX, and from then on `ORACLE(0x4082C0)` is simply
// the exe function at its own address: calls between exe functions (E8 rel32), .rdata constants and .data/.bss globals all resolve without any relocation.
// Not oracle-able: code that calls an import (the IAT is not populated) or a CRT service that needs thread/process state. tools/oracle_closure.py
// reports that statically per root; at run time a vectored exception handler turns a fault inside an oracle call into a "NOT ORACLE-ABLE" report.
// Individual CRT callees can be replaced with a host function (Patch), e.g. the exe's rand() (0x821B1E) -> the test's own rand().
//
// Calling conventions are part of the function type: oracle::Fn<float __cdecl(float)>(0x53CB00), or `float (__cdecl*f)(float) = ORACLE(0x53CB00)`.
// A thiscall method is declared as __fastcall with a dummy 2nd (EDX) argument: oracle::Fn<void __fastcall(CVector*, int)>(0x59C910)(&v, 0)
// (MSVC does not accept __thiscall on a free function type). stdcall: oracle::Fn<bool __stdcall(const CVector*, const CVector*)>(0x412700).
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace oracle {

inline const char* g_where = "";    // what the current test group is doing (for the fault report)
inline bool        g_active = false;

inline LONG CALLBACK FaultHandler(EXCEPTION_POINTERS* ep) {
    const DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_PRIV_INSTRUCTION || c == EXCEPTION_STACK_OVERFLOW || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        std::printf("\n  NOT ORACLE-ABLE: fault 0x%08lX at EIP=%p (access address %p) while running '%s'\n", c, ep->ExceptionRecord->ExceptionAddress,
                    c == EXCEPTION_ACCESS_VIOLATION ? (void*)ep->ExceptionRecord->ExceptionInformation[1] : nullptr, g_where);
        const CONTEXT* cx = ep->ContextRecord;
        std::printf("  EAX=%08lX ECX=%08lX EDX=%08lX ESP=%08lX stack:", cx->Eax, cx->Ecx, cx->Edx, cx->Esp);
        for (int i = 0; i < 6; ++i) std::printf(" %08lX", ((const unsigned long*)cx->Esp)[i]);
        std::printf("\n");
        std::fflush(stdout);
        std::exit(3);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// Copies all sections of the exe to their original VAs. Returns false (and prints why) if the process does not own the range.
inline bool Map(const char* path) {
    FILE* fh = std::fopen(path, "rb");
    if (!fh) { std::printf("  oracle: cannot open %s\n", path); return false; }
    std::vector<uint8_t> exe(0x600000);
    exe.resize(std::fread(exe.data(), 1, exe.size(), fh));
    std::fclose(fh);
    if (exe.size() < 0x1000 || exe[0] != 'M') return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(exe.data() + reinterpret_cast<const IMAGE_DOS_HEADER*>(exe.data())->e_lfanew);
    const uintptr_t base = nt->OptionalHeader.ImageBase;
    if (base != 0x400000) { std::printf("  oracle: unexpected image base\n"); return false; }
    DWORD old = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(0x401000), 0xCB0000 - 0x401000, PAGE_EXECUTE_READWRITE, &old)) {
        std::printf("  oracle: cannot make the placeholder image RWX (error %lu): link with orig_image_pad.obj first and /BASE:0x400000 /FIXED\n", GetLastError());
        return false;
    }
    const auto* sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const uintptr_t va = base + sec[i].VirtualAddress;
        if (va + sec[i].SizeOfRawData > 0xCB0000 || va < 0x401000) continue;   // .rsrc and anything outside the placeholder
        const size_t n = sec[i].SizeOfRawData < sec[i].Misc.VirtualSize ? sec[i].SizeOfRawData : sec[i].Misc.VirtualSize;
        if (sec[i].PointerToRawData + n > exe.size()) continue;
        std::memcpy(reinterpret_cast<void*>(va), exe.data() + sec[i].PointerToRawData, n);
    }
    AddVectoredExceptionHandler(1, FaultHandler);
    return true;
}

// Redirects an exe function to a host function (same calling convention!): overwrites its first 5 bytes with a JMP.
inline void Patch(unsigned va, void* host) {
    auto* p = reinterpret_cast<uint8_t*>(va);
    p[0] = 0xE9;
    *reinterpret_cast<int32_t*>(p + 1) = static_cast<int32_t>(reinterpret_cast<uintptr_t>(host) - (va + 5));
}

struct Addr {
    unsigned va;
    template<class F> operator F*() const { return reinterpret_cast<F*>(static_cast<uintptr_t>(va)); }
};
#define ORACLE(va) (::oracle::Addr{ static_cast<unsigned>(va) })

// explicit form (works where an inline cast is awkward): oracle::Fn<float __cdecl(float)>(0x53CB00)(x)
template<class F> inline F* Fn(unsigned va) { return reinterpret_cast<F*>(static_cast<uintptr_t>(va)); }

} // namespace oracle
