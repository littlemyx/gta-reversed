// ORACLE-W2: allocator / lock / TLS import redirection for the exe-oracle, and a deterministic allocator shared by BOTH sides.
//
// Problem: ORACLE_COVERAGE.md class D = the exe function's call closure reaches a kernel32 import. 773 of the 2785 such functions only reach the CRT heap (operator new / delete / malloc / free ->
// HeapAlloc / HeapFree / HeapReAlloc / HeapSize, the CRT's critical sections and TLS / last-error helpers). The plain oracle leaves the exe's import table (IAT) as read from the file, so the first
// `call dword ptr [IAT]` jumps to garbage.
//
// Exe side (InstallExe):
//   1. CRT entry points are Patch()ed (5-byte JMP) to arena functions: malloc 0x824257, realloc 0x824269, calloc 0x824416, free 0x82413F, operator new 0x82119A (+ the new[] alias 0x821195).
//      operator delete / delete[] (0x8214BD / 0x8213AE) and CMemoryMgr::Free are `jmp 0x82413F`, CMemoryMgr::Malloc/Realloc/Calloc/MallocAlign call the CRT entries: nothing else to patch.
//   2. The IAT slots of the heap / critical-section / TLS / last-error / interlocked imports are pointed at stdcall stubs below (back-stop for CRT paths that are not behind the entry points:
//      _heap_alloc, __sbh_*, _lock/_unlock, _getptd's TLS access, the CRT's own HeapFree).
// Port side: the test defines the replaceable global operator new / delete through ORACLE_ALLOC_HOST_OPERATORS() so that the port's `new` goes to the SAME arena while routing is on.
//
// Determinism: an allocation is a bump of a fixed 16-byte-aligned arena (static storage, never moves: /FIXED, no ASLR). Begin(seed) resets the bump pointer and re-fills the used part with a
// seed-derived byte pattern, so the same call sequence yields the same addresses AND the same "uninitialised" bytes on both sides: a member the exe's constructor writes and the port's leaves
// alone (or the reverse) shows up as a difference of the arena contents. free() is a no-op inside the arena (the whole arena is recycled by the next Begin). A call sequence that differs between
// the sides (the port allocates a temporary the exe does not) shifts every later address: that is reported as a mismatch and judged on the asm, never masked.
#pragma once
#include "game_oracle.h"
#include <new>

namespace oracle::alloc {

inline constexpr size_t kArenaBytes = 8u << 20;
alignas(4096) inline uint8_t g_arena[kArenaBytes];
inline size_t   g_top = 0;           // bump pointer (offset)
inline size_t   g_high = 0;          // highest top ever reached since process start (the fill window)
inline bool     g_route = false;     // host-side operator new / delete -> arena
inline bool     g_overflow = false;
inline uint32_t g_seed = 1;
inline int      g_allocs = 0, g_frees = 0;

inline bool InArena(const void* p) { return p >= g_arena && p < g_arena + kArenaBytes; }

inline void FillPattern(uint8_t* p, size_t n, uint32_t seed) {
    uint32_t s = seed * 2654435761u + 0x9E3779B9u;
    for (size_t i = 0; i + 4 <= n; i += 4) {
        s = s * 1664525u + 1013904223u;
        // mostly "dirty" bytes; every 8th dword zero, so both zero and non-zero garbage are present
        const uint32_t v = ((i >> 2) & 7) == 0 ? 0u : (s ^ (s >> 15)) | 0x01010101u;
        std::memcpy(p + i, &v, 4);
    }
}

// start a deterministic allocation window: arena empty and garbage-filled from `seed`
inline void Begin(uint32_t seed) {
    FillPattern(g_arena, g_high + 256 < kArenaBytes ? g_high + 256 : kArenaBytes, seed);
    g_top = 0; g_seed = seed; g_overflow = false; g_allocs = g_frees = 0; g_route = true;
}
inline void End() { g_route = false; }
inline size_t Used() { return g_top; }

struct Hdr { uint32_t size; uint32_t magic; uint32_t pad[2]; };
inline void* Alloc(size_t n) {
    const size_t need = sizeof(Hdr) + ((n + 15) & ~size_t(15));
    if (g_top + need > kArenaBytes) { g_overflow = true; return nullptr; }
    auto* h = reinterpret_cast<Hdr*>(g_arena + g_top);
    g_top += need;
    if (g_top > g_high) g_high = g_top;
    h->size = (uint32_t)n; h->magic = 0xA110CA7Eu;
    ++g_allocs;
    return h + 1;
}
inline size_t SizeOf(const void* p) { return p && InArena(p) ? reinterpret_cast<const Hdr*>(p)[-1].size : 0; }
inline void Free(void* p) { if (!p) return; if (InArena(p)) { ++g_frees; return; } std::free(p); }
inline void* Realloc(void* p, size_t n) {
    if (!p) return Alloc(n);
    if (!InArena(p)) return std::realloc(p, n);
    void* q = Alloc(n);
    if (q) std::memcpy(q, p, SizeOf(p) < n ? SizeOf(p) : n);
    return q;
}
inline void* Calloc(size_t a, size_t b) { void* p = Alloc(a * b); if (p) std::memset(p, 0, a * b); return p; }

// ---- exe side: CRT entry points (cdecl) -------------------------------------------------------------------------------------------------
inline void* __cdecl ExeMalloc(size_t n)             { return Alloc(n); }
inline void  __cdecl ExeFree(void* p)                { Free(p); }
inline void* __cdecl ExeRealloc(void* p, size_t n)   { return Realloc(p, n); }
inline void* __cdecl ExeCalloc(size_t a, size_t b)   { return Calloc(a, b); }
inline void* __cdecl ExeNew(size_t n)                { return Alloc(n); }       // the exe's operator new loops on _callnewh: with an arena that does not fill up it never fails

// ---- exe side: IAT stubs (stdcall) ------------------------------------------------------------------------------------------------------
inline uint32_t g_fakeTls[128];
inline uint32_t g_fakeLastError = 0;
inline LPVOID  WINAPI SHeapAlloc(HANDLE, DWORD flags, SIZE_T n) { void* p = Alloc(n); if (p && (flags & HEAP_ZERO_MEMORY)) std::memset(p, 0, n); return p; }
inline BOOL    WINAPI SHeapFree(HANDLE, DWORD, LPVOID p)        { Free(p); return TRUE; }
inline LPVOID  WINAPI SHeapReAlloc(HANDLE, DWORD, LPVOID p, SIZE_T n) { return Realloc(p, n); }
inline SIZE_T  WINAPI SHeapSize(HANDLE, DWORD, LPCVOID p)       { return SizeOf(p); }
inline HANDLE  WINAPI SGetProcessHeap()                          { return reinterpret_cast<HANDLE>(1); }
inline void    WINAPI SCs(LPCRITICAL_SECTION)                    {}
inline BOOL    WINAPI SInitCs(LPCRITICAL_SECTION)                { return TRUE; }
inline BOOL    WINAPI SInitCsSpin(LPCRITICAL_SECTION, DWORD)     { return TRUE; }
inline BOOL    WINAPI STryCs(LPCRITICAL_SECTION)                 { return TRUE; }
inline LPVOID  WINAPI STlsGet(DWORD i)                           { return reinterpret_cast<LPVOID>(g_fakeTls[i & 127]); }
inline BOOL    WINAPI STlsSet(DWORD i, LPVOID v)                 { g_fakeTls[i & 127] = reinterpret_cast<uint32_t>(v); return TRUE; }
inline DWORD   WINAPI SGetLastError()                            { return g_fakeLastError; }
inline void    WINAPI SSetLastError(DWORD e)                     { g_fakeLastError = e; }
inline DWORD   WINAPI SGetTid()                                  { return 1; }
inline LONG    WINAPI SIncr(volatile LONG* p)                    { return ++*p; }
inline LONG    WINAPI SDecr(volatile LONG* p)                    { return --*p; }
inline LONG    WINAPI SXchg(volatile LONG* p, LONG v)            { const LONG o = *p; *p = v; return o; }

struct ImportStub { const char* name; void* fn; };
inline const ImportStub kStubs[] = {
    {"HeapAlloc", (void*)&SHeapAlloc}, {"HeapFree", (void*)&SHeapFree}, {"HeapReAlloc", (void*)&SHeapReAlloc}, {"HeapSize", (void*)&SHeapSize}, {"GetProcessHeap", (void*)&SGetProcessHeap},
    {"EnterCriticalSection", (void*)&SCs}, {"LeaveCriticalSection", (void*)&SCs}, {"DeleteCriticalSection", (void*)&SCs}, {"InitializeCriticalSection", (void*)&SCs},
    {"InitializeCriticalSectionAndSpinCount", (void*)&SInitCsSpin}, {"TryEnterCriticalSection", (void*)&STryCs},
    {"TlsGetValue", (void*)&STlsGet}, {"TlsSetValue", (void*)&STlsSet}, {"FlsGetValue", (void*)&STlsGet}, {"FlsSetValue", (void*)&STlsSet},
    {"GetLastError", (void*)&SGetLastError}, {"SetLastError", (void*)&SSetLastError}, {"GetCurrentThreadId", (void*)&SGetTid},
    {"InterlockedIncrement", (void*)&SIncr}, {"InterlockedDecrement", (void*)&SDecr}, {"InterlockedExchange", (void*)&SXchg},
};

// Writes the stubs into the exe's IAT slots (inside the mapped range). Returns the number of slots redirected. `path` = the exe file (its import directory names the slots).
inline int PatchImports(const char* path) {
    FILE* fh = std::fopen(path, "rb");
    if (!fh) return 0;
    std::vector<uint8_t> exe(0x600000);
    exe.resize(std::fread(exe.data(), 1, exe.size(), fh));
    std::fclose(fh);
    if (exe.size() < 0x1000) return 0;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(exe.data() + reinterpret_cast<const IMAGE_DOS_HEADER*>(exe.data())->e_lfanew);
    const auto* sec = IMAGE_FIRST_SECTION(nt);
    auto off = [&](DWORD rva) -> const uint8_t* {
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i)
            if (rva >= sec[i].VirtualAddress && rva < sec[i].VirtualAddress + (sec[i].Misc.VirtualSize > sec[i].SizeOfRawData ? sec[i].Misc.VirtualSize : sec[i].SizeOfRawData))
                return exe.data() + sec[i].PointerToRawData + (rva - sec[i].VirtualAddress);
        return nullptr;
    };
    const DWORD dirRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    const auto* d = dirRva ? reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(off(dirRva)) : nullptr;
    int bound = 0;
    for (; d && d->Name; ++d) {
        const char* dll = reinterpret_cast<const char*>(off(d->Name));
        if (!dll || _stricmp(dll, "kernel32.dll") != 0) continue;
        const auto* thunk = reinterpret_cast<const DWORD*>(off(d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        for (unsigned k = 0; thunk && thunk[k]; ++k) {
            if (thunk[k] & 0x80000000u) continue;
            const char* nm = reinterpret_cast<const char*>(off(thunk[k])) + 2;
            for (const auto& st : kStubs)
                if (!std::strcmp(st.name, nm)) { *reinterpret_cast<uintptr_t*>(0x400000 + d->FirstThunk + 4 * k) = reinterpret_cast<uintptr_t>(st.fn); ++bound; }
        }
    }
    return bound;
}

// exe CRT entry points -> arena; call once after oracle::Map. Returns the number of IAT slots redirected.
inline int InstallExe(const char* exePath) {
    Patch(0x824257, (void*)&ExeMalloc);
    Patch(0x824269, (void*)&ExeRealloc);
    Patch(0x824416, (void*)&ExeCalloc);
    Patch(0x82413F, (void*)&ExeFree);
    Patch(0x82119A, (void*)&ExeNew);
    Patch(0x821195, (void*)&ExeNew);
    return PatchImports(exePath);
}

} // namespace oracle::alloc

// Defines the replaceable global allocation functions of the TEST process: routed to the arena inside a Begin()/End() window, the CRT heap otherwise (harness vectors, strings ...).
// Use once in the test's main .cpp.
#define ORACLE_ALLOC_HOST_OPERATORS()                                                                                                                          \
    void* operator new(size_t n) { if (::oracle::alloc::g_route) { if (void* p = ::oracle::alloc::Alloc(n)) return p; throw std::bad_alloc(); } if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); } \
    void* operator new[](size_t n) { return operator new(n); }                                                                                                  \
    void* operator new(size_t n, const std::nothrow_t&) noexcept { if (::oracle::alloc::g_route) return ::oracle::alloc::Alloc(n); return std::malloc(n ? n : 1); } \
    void* operator new[](size_t n, const std::nothrow_t&) noexcept { return operator new(n, std::nothrow); }                                                    \
    void operator delete(void* p) noexcept { ::oracle::alloc::Free(p); }                                                                                          \
    void operator delete[](void* p) noexcept { ::oracle::alloc::Free(p); }                                                                                         \
    void operator delete(void* p, size_t) noexcept { ::oracle::alloc::Free(p); }                                                                                   \
    void operator delete[](void* p, size_t) noexcept { ::oracle::alloc::Free(p); }
